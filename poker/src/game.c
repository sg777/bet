#include "bet.h"
#include "game.h"
#include "err.h"
#include "poker_vdxf.h"
#include "commands.h"
#include "vdxf.h"
#include <time.h>

// External references
extern char player_ids[CARDS_MAXPLAYERS][MAX_ID_LEN];
extern int32_t num_of_players;

// Card type names for display
static const char *get_card_type_str(int32_t card_type) {
	switch (card_type) {
		case 1: return "HOLE";
		case 2: return "FLOP-1";
		case 3: return "FLOP-2";
		case 4: return "FLOP-3";
		case 5: return "TURN";
		case 6: return "RIVER";
		default: return "???";
	}
}

struct t_game_info_struct t_game_info;

// Global start_block for reading combined CMM view
// Set by dealer during init, by player when joining table
int32_t g_start_block = 0;

const char *game_state_str(int32_t game_state)
{
	switch (game_state) {
	case G_ZEROIZED_STATE:
		return "Zeroized state, Table is not initialized yet...";
	case G_TABLE_ACTIVE:
		return "Table is active";
	case G_TABLE_STARTED:
		return "Table is started";
	case G_PLAYERS_JOINED:
		return "Players joined the table";
	case G_DECK_SHUFFLING_P:
		return "Deck shuffling by players are done";
	case G_DECK_SHUFFLING_D:
		return "Deck shuffling by dealer is done";
	case G_DECK_SHUFFLING_B:
		return "Deck shuffling by cashier is done";
	case G_REVEAL_CARD_P_DONE:
		return "Player(s) got the card";
	case G_REVEAL_CARD:
		return "Drawing the card from deck";
	case G_ROUND_BETTING:
		return "Round betting is happening...";
	case G_SHOWDOWN:
		return "Showdown - determining winner";
	case G_SETTLEMENT_PENDING:
		return "Settlement pending - cashier processing payouts";
	case G_SETTLEMENT_COMPLETE:
		return "Settlement complete - game finished";
	default:
		return "Invalid game state...";
	}
}

cJSON *append_game_state(const char *id, int32_t game_state, cJSON *game_state_info)
{
	char *game_id_str = NULL;
	cJSON *t_game_info = NULL, *out = NULL;

	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);
	if (!game_id_str) {
		dlg_warn("Probably the ID is not initialized");
		return NULL;
	}
	t_game_info = cJSON_CreateObject();
	cJSON_AddNumberToObject(t_game_info, "game_state", game_state);
	if (game_state_info)
		cJSON_AddItemToObject(t_game_info, "game_state_info", game_state_info);

	out = poker_append_key_json(id, get_key_data_vdxf_id(T_GAME_INFO_KEY, game_id_str),
					t_game_info, true);
	return out;
}

int32_t get_game_state(const char *id)
{
	int32_t game_state = G_ZEROIZED_STATE;
	char *game_id_str = NULL;
	cJSON *t_game_info = NULL;
	int32_t height_start = g_start_block;

	// If g_start_block is 0, use current_block - 100 as fallback
	if (height_start <= 0) {
		height_start = chips_get_block_count() - 100;
		if (height_start < 0) height_start = 0;
	}

	// Use getidentitycontent to get COMBINED view of all CMM updates from height_start
	game_id_str = get_str_from_id_key_from_height(id, T_GAME_ID_KEY, height_start);
	if (!game_id_str) {
		/*
			Game ID is NULL, it probably mean the ID hasn't been initialized yet, so game state is in zeroized state.
		*/
		return game_state;
	}

	t_game_info = get_cJSON_from_id_key_vdxfid_from_height(id, 
		get_key_data_vdxf_id(T_GAME_INFO_KEY, game_id_str), height_start);
	if (!t_game_info)
		return game_state;

	game_state = jint(t_game_info, "game_state");
	return game_state;
}

cJSON *get_game_state_info(const char *id)
{
	char *game_id_str = NULL;
	cJSON *t_game_info = NULL, *game_state_info = NULL;

	game_id_str = poker_get_key_str(id, T_GAME_ID_KEY);
	if (!game_id_str)
		return NULL;

	t_game_info = get_cJSON_from_id_key_vdxfid_from_height(id, get_key_data_vdxf_id(T_GAME_INFO_KEY, game_id_str), g_start_block);
	if (!t_game_info)
		return NULL;

	game_state_info = jobj(t_game_info, "game_state_info");
	return game_state_info;
}

void init_struct_vars()
{
	dcv_vars = calloc(1, sizeof(struct privatebet_vars));

	dcv_vars->turni = 0;
	dcv_vars->round = 0;
	dcv_vars->pot = 0.0;
	dcv_vars->last_turn = 0;
	dcv_vars->last_raise = 0.0;
	dcv_vars->small_blind = 0.01;  // 0.01 CHIPS
	dcv_vars->big_blind = 0.02;    // 0.02 CHIPS
	dcv_vars->dealer = 0;  // Player 0 is dealer (SB), Player 1 is BB
	dcv_vars->turn_start_time = 0;
	dcv_vars->turn_start_block = 0;
	for (int i = 0; i < CARDS_MAXPLAYERS; i++) {
		dcv_vars->funds[i] = 0.0;  // Will be set from payin amounts
		dcv_vars->ini_funds[i] = 0.0;
		dcv_vars->win_funds[i] = 0.0;
		dcv_vars->funds_spent[i] = 0.0;
		dcv_vars->winners[i] = 0;
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			dcv_vars->bet_actions[i][j] = 0;
			dcv_vars->betamount[i][j] = 0.0;
		}
	}
	no_of_cards = 0;
}

int32_t init_game_meta_info(char *table_id)
{
	int32_t retval = OK;
	char *game_id_str = NULL;
	cJSON *t_player_info = NULL, *payin_amounts = NULL;

	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	t_player_info = get_cJSON_from_id_key_vdxfid_from_height(table_id, get_key_data_vdxf_id(T_PLAYER_INFO_KEY, game_id_str), g_start_block);

	game_meta_info.num_players = jint(t_player_info, "num_players");
	game_meta_info.dealer_pos = 0;
	game_meta_info.turn = (game_meta_info.dealer_pos + 1) % game_meta_info.num_players;
	game_meta_info.card_id = 0;

	for (int32_t i = 0; i < game_meta_info.num_players; i++) {
		for (int32_t j = 0; j < hand_size; j++) {
			game_meta_info.card_state[i][j] = no_card_drawn;
		}
	}
	
	// Load player funds from payin_amounts (actual CHIPS values)
	dlg_info("Loading player funds from t_player_info...");
	if (t_player_info) {
		payin_amounts = cJSON_GetObjectItem(t_player_info, "payin_amounts");
		if (payin_amounts && payin_amounts->type == cJSON_Array) {
			dlg_info("Found payin_amounts array with %d elements", cJSON_GetArraySize(payin_amounts));
			for (int32_t i = 0; i < cJSON_GetArraySize(payin_amounts) && i < CARDS_MAXPLAYERS; i++) {
				cJSON *item = cJSON_GetArrayItem(payin_amounts, i);
				if (item) {
					double payin_chips = item->valuedouble;
					dcv_vars->funds[i] = payin_chips;      // Direct CHIPS value
					dcv_vars->ini_funds[i] = payin_chips;  // Track initial amount
					dlg_info("Player %d (%s): payin %.4f CHIPS", i, player_ids[i], payin_chips);
				}
			}
		} else {
			dlg_warn("No payin_amounts found in t_player_info, using default 0.50 CHIPS");
			for (int32_t i = 0; i < game_meta_info.num_players; i++) {
				dcv_vars->funds[i] = 0.50;      // Default 0.50 CHIPS
				dcv_vars->ini_funds[i] = 0.50;
			}
		}
	} else {
		dlg_warn("t_player_info is NULL, using default 0.50 CHIPS");
		for (int32_t i = 0; i < game_meta_info.num_players; i++) {
			dcv_vars->funds[i] = 0.50;
			dcv_vars->ini_funds[i] = 0.50;
		}
	}
	
	return retval;
}

int32_t is_card_drawn(char *table_id)
{
	int32_t retval = OK;
	cJSON *game_state_info = NULL, *player_game_state_info = NULL;

	game_state_info = get_game_state_info(table_id);
	int32_t target_player = jint(game_state_info, "player_id");
	int32_t target_card = jint(game_state_info, "card_id");
	int32_t card_type = jint(game_state_info, "card_type");
	
	// Record start time for timeout
	int64_t start_time = (int64_t)time(NULL);
	int32_t start_block = chips_get_block_count();
	
	dlg_info("╔═══════════════════════════════════════════════════════╗");
	dlg_info("║  Waiting for player %s to reveal card...    ║", player_ids[target_player]);
	dlg_info("║  Card ID: %d, Type: %-8s                          ║", target_card, get_card_type_str(card_type));
	dlg_info("╚═══════════════════════════════════════════════════════╝");
	
	while (1) {
		// Check for timeout
		int64_t elapsed_secs = (int64_t)time(NULL) - start_time;
		int32_t elapsed_blocks = chips_get_block_count() - start_block;
		
		if (elapsed_secs >= BET_TURN_TIMEOUT_SECS && elapsed_blocks >= BET_TURN_TIMEOUT_BLOCKS) {
			dlg_warn("═══════════════════════════════════════════");
			dlg_warn("  ⏰ TIMEOUT - Player %s didn't reveal card!", player_ids[target_player]);
			dlg_warn("  Elapsed: %lld secs, %d blocks", (long long)elapsed_secs, elapsed_blocks);
			dlg_warn("  Treating as FOLD - player removed from game");
			dlg_warn("═══════════════════════════════════════════");
			// Mark player as folded for all rounds
			for (int r = 0; r < CARDS_MAXROUNDS; r++) {
				dcv_vars->bet_actions[target_player][r] = fold;
			}
			return ERR_PLAYER_TIMEOUT;
		}
		
		player_game_state_info = get_game_state_info(player_ids[target_player]);
		if (!player_game_state_info) {
			sleep(2);
			continue;
		}
		if ((target_player == jint(player_game_state_info, "player_id")) &&
		    (target_card == jint(player_game_state_info, "card_id"))) {
			dlg_info("✓ Player %s confirmed card %d", player_ids[target_player], target_card);
			break;
		}
		sleep(2);
	}
	return retval;
}

static int32_t update_next_card(char *table_id, int32_t player_id, int32_t card_id, int32_t card_type)
{
	cJSON *game_state_info = NULL, *out = NULL;
	int32_t retval = OK;

	game_state_info = cJSON_CreateObject();
	cJSON_AddNumberToObject(game_state_info, "player_id", player_id);
	cJSON_AddNumberToObject(game_state_info, "card_id", card_id);
	cJSON_AddNumberToObject(game_state_info, "card_type", card_type);

	dlg_info("%s", cJSON_Print(game_state_info));

	out = append_game_state(table_id, G_REVEAL_CARD, game_state_info);
	if (!out)
		retval = ERR_UPDATEIDENTITY;

	return retval;
}

int32_t deal_next_card(char *table_id)
{
	int32_t retval = OK;

	if (hole_cards_drawn == 0) {
		for (int i = 0; i < no_of_hole_cards; i++) {
			for (int j = 0; j < num_of_players; j++) {
				if (card_matrix[j][i] == 0) {
					retval = update_next_card(table_id, j, (i * num_of_players) + j, hole_card);
					goto end;
				}
			}
		}
	} else if (flop_cards_drawn == 0) {
		for (int i = no_of_hole_cards; i < no_of_hole_cards + no_of_flop_cards; i++) {
			for (int j = 0; j < num_of_players; j++) {
				if (card_matrix[j][i] == 0) {
					if ((i - (no_of_hole_cards)) == 0) {
						retval = update_next_card(table_id, j,
									  (no_of_hole_cards * num_of_players) +
										  (i - no_of_hole_cards) + 1,
									  flop_card_1);
					} else if ((i - (no_of_hole_cards)) == 1) {
						retval = update_next_card(table_id, j,
									  (no_of_hole_cards * num_of_players) +
										  (i - no_of_hole_cards) + 1,
									  flop_card_2);
					} else if ((i - (no_of_hole_cards)) == 2) {
						retval = update_next_card(table_id, j,
									  (no_of_hole_cards * num_of_players) +
										  (i - no_of_hole_cards) + 1,
									  flop_card_3);
					}
					goto end;
				}
			}
		}
	} else if (turn_card_drawn == 0) {
		for (int i = no_of_hole_cards + no_of_flop_cards;
		     i < no_of_hole_cards + no_of_flop_cards + no_of_turn_card; i++) {
			for (int j = 0; j < num_of_players; j++) {
				if (card_matrix[j][i] == 0) {
					retval = update_next_card(table_id, j,
								  (no_of_hole_cards * num_of_players) +
									  (i - no_of_hole_cards) + 2,
								  turn_card);
					goto end;
				}
			}
		}
	} else if (river_card_drawn == 0) {
		for (int i = no_of_hole_cards + no_of_flop_cards + no_of_turn_card;
		     i < no_of_hole_cards + no_of_flop_cards + no_of_turn_card + no_of_river_card; i++) {
			for (int j = 0; j < num_of_players; j++) {
				if (card_matrix[j][i] == 0) {
					retval = update_next_card(table_id, j,
								  (no_of_hole_cards * num_of_players) +
									  (i - no_of_hole_cards) + 3,
								  river_card);
					goto end;
				}
			}
		}
	} else
		retval = ERR_ALL_CARDS_DRAWN;
end:
	return retval;
}

int32_t init_game_state(char *table_id)
{
	int32_t retval = OK;

	init_struct_vars();
	init_game_meta_info(table_id);  // Load player funds from payin amounts
	retval = deal_next_card(table_id);
	return retval;
}

int32_t verus_receive_card(char *table_id, struct privatebet_vars *vars)
{
	int retval = OK, playerid, cardid, card_type, flag = 1;
	cJSON *game_state_info = NULL;

	game_state_info = get_game_state_info(table_id);

	playerid = jint(game_state_info, "player_id");
	cardid = jint(game_state_info, "card_id");
	card_type = jint(game_state_info, "card_type");

	no_of_cards++;

	dlg_info("no_of_cards drawn :: %d", no_of_cards);

	if (card_type == hole_card) {
		card_matrix[(cardid % num_of_players)][(cardid / num_of_players)] = 1;
		//card_values[(cardid % bet->maxplayers)][(cardid / bet->maxplayers)] = jint(player_card_info, "decoded_card");
	} else if (card_type == flop_card_1) {
		card_matrix[playerid][no_of_hole_cards] = 1;
		//card_values[playerid][no_of_hole_cards] = jint(player_card_info, "decoded_card");
	} else if (card_type == flop_card_2) {
		card_matrix[playerid][no_of_hole_cards + 1] = 1;
		//card_values[playerid][no_of_hole_cards + 1] = jint(player_card_info, "decoded_card");
	} else if (card_type == flop_card_3) {
		card_matrix[playerid][no_of_hole_cards + 2] = 1;
		//card_values[playerid][no_of_hole_cards + 2] = jint(player_card_info, "decoded_card");
	} else if (card_type == turn_card) {
		card_matrix[playerid][no_of_hole_cards + no_of_flop_cards] = 1;
		//card_values[playerid][no_of_hole_cards + no_of_flop_cards] = jint(player_card_info, "decoded_card");
	} else if (card_type == river_card) {
		card_matrix[playerid][no_of_hole_cards + no_of_flop_cards + no_of_turn_card] = 1;
		//card_values[playerid][no_of_hole_cards + no_of_flop_cards + no_of_turn_card] = jint(player_card_info, "decoded_card");
	}

	if (hole_cards_drawn == 0) {
		flag = 1;
		for (int i = 0; ((i < no_of_hole_cards) && (flag)); i++) {
			for (int j = 0; ((j < num_of_players) && (flag)); j++) {
				if (card_matrix[j][i] == 0) {
					flag = 0;
				}
			}
		}
		if (flag) {
			hole_cards_drawn = 1;
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  ✓ ALL HOLE CARDS DEALT - PREFLOP BETTING  ");
			dlg_info("═══════════════════════════════════════════");
		}

	} else if (flop_cards_drawn == 0) {
		flag = 1;
		for (int i = no_of_hole_cards; ((i < no_of_hole_cards + no_of_flop_cards) && (flag)); i++) {
			for (int j = 0; ((j < num_of_players) && (flag)); j++) {
				if (card_matrix[j][i] == 0) {
					flag = 0;
				}
			}
		}
		if (flag) {
			flop_cards_drawn = 1;
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  ✓ FLOP DEALT - FLOP BETTING               ");
			dlg_info("═══════════════════════════════════════════");
			// Update board cards on table ID for each flop card
			update_board_cards(table_id, flop_card_1);
			update_board_cards(table_id, flop_card_2);
			update_board_cards(table_id, flop_card_3);
		}

	} else if (turn_card_drawn == 0) {
		flag = 1;
		for (int i = no_of_hole_cards + no_of_flop_cards;
		     ((i < no_of_hole_cards + no_of_flop_cards + no_of_turn_card) && (flag)); i++) {
			for (int j = 0; ((j < num_of_players) && (flag)); j++) {
				if (card_matrix[j][i] == 0) {
					flag = 0;
				}
			}
		}
		if (flag) {
			turn_card_drawn = 1;
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  ✓ TURN DEALT - TURN BETTING               ");
			dlg_info("═══════════════════════════════════════════");
			// Update board cards on table ID
			update_board_cards(table_id, turn_card);
		}

	} else if (river_card_drawn == 0) {
		flag = 1;
		for (int i = no_of_hole_cards + no_of_flop_cards + no_of_turn_card;
		     ((i < no_of_hole_cards + no_of_flop_cards + no_of_turn_card + no_of_river_card) && (flag)); i++) {
			for (int j = 0; ((j < num_of_players) && (flag)); j++) {
				if (card_matrix[j][i] == 0) {
					flag = 0;
				}
			}
		}
		if (flag) {
			river_card_drawn = 1;
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  ✓ RIVER DEALT - FINAL BETTING             ");
			dlg_info("═══════════════════════════════════════════");
			// Update board cards on table ID
			update_board_cards(table_id, river_card);
		}
	}

	if (flag) {
		// Check if all community cards are dealt (river done)
		if (river_card_drawn == 1) {
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  🏆 ALL CARDS DEALT - PROCEEDING TO SHOWDOWN ");
			dlg_info("═══════════════════════════════════════════");
			append_game_state(table_id, G_SHOWDOWN, NULL);
		} else if (vars->round == 0) {
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  💰 INITIATING BETTING - SMALL BLIND       ");
			dlg_info("═══════════════════════════════════════════");
			retval = verus_small_blind(table_id, vars);
		} else {
			// Initiate betting for subsequent rounds (flop, turn, river)
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  💰 INITIATING BETTING - ROUND %d          ", vars->round + 1);
			dlg_info("═══════════════════════════════════════════");
			retval = verus_write_betting_state(table_id, vars, "round_betting");
		}
	} else {
		retval = deal_next_card(table_id);
	}

	return retval;
}

// Poll all players for their decoded community card value and update table's board cards
int32_t update_board_cards(char *table_id, int32_t card_type)
{
	int32_t retval = OK, consensus_value = -1, confirmed_count = 0;
	char *game_id_str = NULL;
	cJSON *board_cards = NULL, *out = NULL, *flop_arr = NULL;

	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	if (!game_id_str) {
		return ERR_GAME_ID_NOT_FOUND;
	}

	// Poll each player for their decoded card value
	for (int32_t i = 0; i < num_of_players; i++) {
		cJSON *player_decoded = get_cJSON_from_id_key_vdxfid_from_height(player_ids[i],
			get_key_data_vdxf_id(P_DECODED_CARD_KEY, game_id_str), g_start_block);
		if (player_decoded) {
			int32_t p_card_type = jint(player_decoded, "card_type");
			int32_t p_card_value = jint(player_decoded, "card_value");
			
			if (p_card_type == card_type) {
				if (consensus_value == -1) {
					consensus_value = p_card_value;
					confirmed_count = 1;
				} else if (p_card_value == consensus_value) {
					confirmed_count++;
				} else {
					dlg_error("Player %d reported different card value for type %d: %d vs %d",
						i, card_type, p_card_value, consensus_value);
					// TODO: Handle dispute
				}
			}
		}
	}

	if (confirmed_count < num_of_players) {
		dlg_info("Not all players confirmed community card yet: %d/%d", confirmed_count, num_of_players);
		return OK; // Will retry on next poll
	}

	dlg_info("All players confirmed community card type %d with value %d", card_type, consensus_value);

	// Get or create board_cards
	board_cards = get_cJSON_from_id_key_vdxfid_from_height(table_id, get_key_data_vdxf_id(T_BOARD_CARDS_KEY, game_id_str), g_start_block);
	if (!board_cards) {
		board_cards = cJSON_CreateObject();
		flop_arr = cJSON_CreateArray();
		cJSON_AddItemToArray(flop_arr, cJSON_CreateNumber(-1));
		cJSON_AddItemToArray(flop_arr, cJSON_CreateNumber(-1));
		cJSON_AddItemToArray(flop_arr, cJSON_CreateNumber(-1));
		cJSON_AddItemToObject(board_cards, "flop", flop_arr);
		cJSON_AddNumberToObject(board_cards, "turn", -1);
		cJSON_AddNumberToObject(board_cards, "river", -1);
	}

	// Update the appropriate field
	switch (card_type) {
	case flop_card_1:
		cJSON_ReplaceItemInArray(cJSON_GetObjectItem(board_cards, "flop"), 0, cJSON_CreateNumber(consensus_value));
		break;
	case flop_card_2:
		cJSON_ReplaceItemInArray(cJSON_GetObjectItem(board_cards, "flop"), 1, cJSON_CreateNumber(consensus_value));
		break;
	case flop_card_3:
		cJSON_ReplaceItemInArray(cJSON_GetObjectItem(board_cards, "flop"), 2, cJSON_CreateNumber(consensus_value));
		break;
	case turn_card:
		cJSON_ReplaceItemInObject(board_cards, "turn", cJSON_CreateNumber(consensus_value));
		break;
	case river_card:
		cJSON_ReplaceItemInObject(board_cards, "river", cJSON_CreateNumber(consensus_value));
		break;
	}

	// Update table ID with board cards
	out = poker_append_key_json(table_id, get_key_data_vdxf_id(T_BOARD_CARDS_KEY, game_id_str), board_cards, true);
	if (!out) {
		dlg_error("Failed to update board cards on table ID");
		retval = ERR_UPDATEIDENTITY;
	} else {
		dlg_info("Updated board cards on table ID: %s", cJSON_Print(board_cards));
	}

	return retval;
}

int32_t verus_small_blind(char *table_id, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *smallBlindInfo = NULL, *out = NULL;

	vars->last_turn = vars->dealer;
	vars->turni = (vars->dealer) % num_of_players;

	smallBlindInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(smallBlindInfo, "method", "betting");
	cJSON_AddStringToObject(smallBlindInfo, "action", "small_blind");
	cJSON_AddNumberToObject(smallBlindInfo, "playerid", vars->turni);
	cJSON_AddNumberToObject(smallBlindInfo, "round", vars->round);
	cJSON_AddNumberToObject(smallBlindInfo, "pot", vars->pot);
	cJSON_AddNumberToObject(smallBlindInfo, "min_amount", vars->small_blind);

	out = append_game_state(table_id, G_ROUND_BETTING, smallBlindInfo);
	dlg_info("═══════════════════════════════════════════");
	dlg_info("  💰 SMALL BLIND - Player %d (%s)          ", vars->turni, player_ids[vars->turni]);
	dlg_info("═══════════════════════════════════════════");
	
	// Also write betting state to TABLE ID
	retval = verus_write_betting_state(table_id, vars, "small_blind");

	return retval;
}

/**
 * Write betting state to TABLE ID for players to read
 */
int32_t verus_write_betting_state(char *table_id, struct privatebet_vars *vars, const char *action)
{
	int32_t retval = OK;
	char *game_id_str = NULL;
	cJSON *betting_state = NULL, *bet_amounts = NULL, *player_funds = NULL;
	cJSON *possibilities = NULL, *out = NULL;
	
	game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	if (!game_id_str) return ERR_GAME_ID_NOT_FOUND;
	
	betting_state = cJSON_CreateObject();
	cJSON_AddNumberToObject(betting_state, "current_turn", vars->turni);
	cJSON_AddNumberToObject(betting_state, "round", vars->round);
	cJSON_AddNumberToObject(betting_state, "pot", vars->pot);
	cJSON_AddStringToObject(betting_state, "action", action);
	cJSON_AddNumberToObject(betting_state, "last_turn", vars->last_turn);
	
	// Record turn start time for timeout tracking
	vars->turn_start_time = (int64_t)time(NULL);
	vars->turn_start_block = chips_get_block_count();
	cJSON_AddNumberToObject(betting_state, "turn_start_time", (double)vars->turn_start_time);
	cJSON_AddNumberToObject(betting_state, "turn_start_block", vars->turn_start_block);
	cJSON_AddNumberToObject(betting_state, "timeout_secs", BET_TURN_TIMEOUT_SECS);
	cJSON_AddNumberToObject(betting_state, "timeout_blocks", BET_TURN_TIMEOUT_BLOCKS);
	
	// Calculate max bet and min amount (in CHIPS)
	double maxbet = 0.0;
	for (int i = 0; i < num_of_players; i++) {
		if (vars->betamount[i][vars->round] > maxbet)
			maxbet = vars->betamount[i][vars->round];
	}
	
	// For blinds use fixed blind amount, for regular betting calculate to call
	double min_amount = 0.0;
	if (strcmp(action, "small_blind") == 0) {
		min_amount = vars->small_blind;  // 0.01 CHIPS
	} else if (strcmp(action, "big_blind") == 0) {
		min_amount = vars->big_blind;    // 0.02 CHIPS
	} else {
		// Regular betting - calculate amount to call
		min_amount = maxbet - vars->betamount[vars->turni][vars->round];
	}
	cJSON_AddNumberToObject(betting_state, "min_amount", min_amount);
	
	// Bet amounts per player
	bet_amounts = cJSON_CreateArray();
	for (int i = 0; i < num_of_players; i++) {
		cJSON_AddItemToArray(bet_amounts, cJSON_CreateNumber(vars->betamount[i][vars->round]));
	}
	cJSON_AddItemToObject(betting_state, "bet_amounts", bet_amounts);
	
	// Player funds
	player_funds = cJSON_CreateArray();
	for (int i = 0; i < num_of_players; i++) {
		cJSON_AddItemToArray(player_funds, cJSON_CreateNumber(vars->funds[i]));
	}
	cJSON_AddItemToObject(betting_state, "player_funds", player_funds);
	
	// Possibilities (what actions are allowed)
	possibilities = cJSON_CreateArray();
	if (strcmp(action, "small_blind") == 0 || strcmp(action, "big_blind") == 0) {
		cJSON_AddItemToArray(possibilities, cJSON_CreateString("bet"));
	} else {
		// Regular betting round
		if (maxbet == vars->betamount[vars->turni][vars->round]) {
			cJSON_AddItemToArray(possibilities, cJSON_CreateString("check"));
		}
		if (maxbet > vars->betamount[vars->turni][vars->round]) {
			cJSON_AddItemToArray(possibilities, cJSON_CreateString("call"));
		}
		cJSON_AddItemToArray(possibilities, cJSON_CreateString("raise"));
		cJSON_AddItemToArray(possibilities, cJSON_CreateString("fold"));
		cJSON_AddItemToArray(possibilities, cJSON_CreateString("allin"));
	}
	cJSON_AddItemToObject(betting_state, "possibilities", possibilities);
	
	dlg_info("Writing betting state: turn=%d, round=%d, pot=%.4f CHIPS, action=%s",
		vars->turni, vars->round, vars->pot, action);
	
	out = poker_update_key_json(table_id, 
		get_key_data_vdxf_id(T_BETTING_STATE_KEY, game_id_str), betting_state, true);
	
	cJSON_Delete(betting_state);
	
	if (!out) {
		return ERR_GAME_STATE_UPDATE;
	}
	
	return retval;
}

/**
 * Poll player's betting action from their ID
 * Returns: action string or NULL if not yet acted
 */
cJSON *verus_poll_player_action(char *table_id, int32_t player_idx, int32_t expected_round)
{
	char *game_id_str = poker_get_key_str(table_id, T_GAME_ID_KEY);
	if (!game_id_str) return NULL;
	
	cJSON *action = get_cJSON_from_id_key_vdxfid_from_height(
		player_ids[player_idx],
		get_key_data_vdxf_id(P_BETTING_ACTION_KEY, game_id_str),
		g_start_block);
	
	if (!action) return NULL;
	
	// Check if action is for current round
	if (jint(action, "round") != expected_round) {
		return NULL;  // Old action from previous round
	}
	
	return action;
}

/**
 * Dealer processes a player's betting action
 * All amounts in CHIPS (e.g., 0.01, 0.02)
 * Validates bet amount against available funds - auto all-in if bet > funds
 */
int32_t verus_process_betting_action(char *table_id, struct privatebet_vars *vars, cJSON *action_info)
{
	int32_t retval = OK;
	const char *action = jstr(action_info, "action");
	double amount = jdouble(action_info, "amount");  // Amount in CHIPS
	int32_t playerid = vars->turni;
	double available = vars->funds[playerid];
	
	dlg_info("═══════════════════════════════════════════");
	dlg_info("  Player %d (%s): %s %.4f CHIPS (available: %.4f)", playerid, player_ids[playerid], action, amount, available);
	dlg_info("═══════════════════════════════════════════");
	
	// Validate: if player tries to bet more than they have, treat as all-in
	if (amount > available && strcmp(action, "fold") != 0 && strcmp(action, "check") != 0) {
		dlg_info("  ⚠️ Bet %.4f exceeds available %.4f - converting to ALL-IN", amount, available);
		amount = available;
		// Force all-in action
		vars->betamount[playerid][vars->round] += amount;
		vars->pot += amount;
		vars->funds[playerid] = 0.0;
		vars->bet_actions[playerid][vars->round] = allin;
		dlg_info("  💰 Pot: %.4f CHIPS, Player funds: %.4f (ALL-IN)", vars->pot, vars->funds[playerid]);
		return retval;
	}
	
	if (strcmp(action, "fold") == 0) {
		vars->bet_actions[playerid][vars->round] = fold;
	} else if (strcmp(action, "call") == 0) {
		double maxbet = 0.0;
		for (int i = 0; i < num_of_players; i++) {
			if (vars->betamount[i][vars->round] > maxbet)
				maxbet = vars->betamount[i][vars->round];
		}
		double to_call = maxbet - vars->betamount[playerid][vars->round];
		// Cap call at available funds
		if (to_call > available) {
			dlg_info("  ⚠️ Call %.4f exceeds available %.4f - ALL-IN", to_call, available);
			to_call = available;
			vars->bet_actions[playerid][vars->round] = allin;
		} else {
			vars->bet_actions[playerid][vars->round] = call;
		}
		vars->betamount[playerid][vars->round] += to_call;
		vars->funds[playerid] -= to_call;
		vars->pot += to_call;
	} else if (strcmp(action, "raise") == 0) {
		// Cap raise at available funds
		if (amount > available) {
			dlg_info("  ⚠️ Raise %.4f exceeds available %.4f - ALL-IN", amount, available);
			amount = available;
			vars->bet_actions[playerid][vars->round] = allin;
		} else {
			vars->bet_actions[playerid][vars->round] = raise;
		}
		vars->betamount[playerid][vars->round] += amount;
		vars->funds[playerid] -= amount;
		vars->pot += amount;
		vars->last_raise = amount;
	} else if (strcmp(action, "check") == 0) {
		vars->bet_actions[playerid][vars->round] = check;
	} else if (strcmp(action, "allin") == 0) {
		double allin_amount = vars->funds[playerid];
		vars->betamount[playerid][vars->round] += allin_amount;
		vars->pot += allin_amount;
		vars->funds[playerid] = 0.0;
		vars->bet_actions[playerid][vars->round] = allin;
	} else if (strcmp(action, "small_blind") == 0 || strcmp(action, "bet") == 0) {
		vars->betamount[playerid][vars->round] = amount;
		vars->funds[playerid] -= amount;
		vars->pot += amount;
		vars->bet_actions[playerid][vars->round] = small_blind;
	} else if (strcmp(action, "big_blind") == 0) {
		vars->betamount[playerid][vars->round] = amount;
		vars->funds[playerid] -= amount;
		vars->pot += amount;
		vars->bet_actions[playerid][vars->round] = big_blind;
	}
	
	dlg_info("  💰 Pot: %.4f CHIPS, Player funds: %.4f", vars->pot, vars->funds[playerid]);
	
	return retval;
}

/**
 * Determine next turn in betting round
 * Returns: next player index, or -1 if round complete
 */
int32_t verus_next_turn(struct privatebet_vars *vars)
{
	int32_t maxamount = 0;
	
	// Find max bet this round
	for (int i = 0; i < num_of_players; i++) {
		if (vars->bet_actions[i][vars->round] != fold) {
			if (vars->betamount[i][vars->round] > maxamount)
				maxamount = vars->betamount[i][vars->round];
		}
	}
	
	// Find next player who needs to act
	for (int i = 1; i <= num_of_players; i++) {
		int idx = (vars->turni + i) % num_of_players;
		
		// Skip folded/allin players
		if (vars->bet_actions[idx][vars->round] == fold ||
		    vars->bet_actions[idx][vars->round] == allin ||
		    vars->funds[idx] == 0)
			continue;
		
		// Player hasn't acted yet
		if (vars->bet_actions[idx][vars->round] == 0)
			return idx;
		
		// Player needs to call a raise
		if (vars->betamount[idx][vars->round] < maxamount)
			return idx;
	}
	
	return -1;  // Round complete
}

/**
 * Check if current player's turn has timed out
 * Returns: 1 if timed out, 0 if still within time limit
 */
int32_t verus_check_turn_timeout(struct privatebet_vars *vars)
{
	int64_t current_time = (int64_t)time(NULL);
	int32_t current_block = chips_get_block_count();
	
	int64_t elapsed_secs = current_time - vars->turn_start_time;
	int32_t elapsed_blocks = current_block - vars->turn_start_block;
	
	// Timeout if BOTH conditions met (whichever is higher)
	// This means we wait for at least 60 secs AND at least 6 blocks
	if (elapsed_secs >= BET_TURN_TIMEOUT_SECS && elapsed_blocks >= BET_TURN_TIMEOUT_BLOCKS) {
		return 1;
	}
	return 0;
}

/**
 * Dealer handles round betting - called from handle_game_state
 */
int32_t verus_handle_round_betting(char *table_id, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *player_action = NULL;
	
	// Poll current player for their action
	player_action = verus_poll_player_action(table_id, vars->turni, vars->round);
	
	if (!player_action) {
		// Player hasn't acted yet - check for timeout
		if (verus_check_turn_timeout(vars)) {
			// TIMEOUT - Auto-fold the player
			int64_t elapsed = (int64_t)time(NULL) - vars->turn_start_time;
			int32_t blocks_elapsed = chips_get_block_count() - vars->turn_start_block;
			
			dlg_warn("═══════════════════════════════════════════");
			dlg_warn("  ⏰ TIMEOUT - Player %d (%s) auto-folded!", vars->turni, player_ids[vars->turni]);
			dlg_warn("  Elapsed: %lld secs, %d blocks", (long long)elapsed, blocks_elapsed);
			dlg_warn("═══════════════════════════════════════════");
			
			// Create auto-fold action
			player_action = cJSON_CreateObject();
			cJSON_AddStringToObject(player_action, "action", "fold");
			cJSON_AddNumberToObject(player_action, "amount", 0);
			cJSON_AddNumberToObject(player_action, "round", vars->round);
			cJSON_AddBoolToObject(player_action, "auto_fold", cJSON_True);
		} else {
			// Still waiting
			return OK;
		}
	}
	
	// Process the action
	retval = verus_process_betting_action(table_id, vars, player_action);
	if (retval != OK) return retval;
	
	// Determine next turn
	int32_t next = verus_next_turn(vars);
	
	if (next == -1) {
		// Round complete
		vars->round++;
		vars->turni = vars->dealer;
		
		// Count players still in
		int32_t players_left = 0;
		for (int i = 0; i < num_of_players; i++) {
			if (vars->bet_actions[i][vars->round - 1] != fold)
				players_left++;
		}
		
		if (vars->round >= CARDS_MAXROUNDS || players_left < 2) {
			// Game over - showdown
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  🏆 SHOWDOWN - Evaluating hands...         ");
			dlg_info("═══════════════════════════════════════════");
			append_game_state(table_id, G_SHOWDOWN, NULL);
		} else {
			// Deal next community cards
			dlg_info("═══════════════════════════════════════════");
			dlg_info("  → Round %d complete, dealing next cards   ", vars->round);
			dlg_info("═══════════════════════════════════════════");
			retval = deal_next_card(table_id);
		}
	} else {
		// Next player's turn
		vars->last_turn = vars->turni;
		vars->turni = next;
		
		// Determine action type for next player
		const char *next_action = "round_betting";
		if (vars->round == 0) {
			// Preflop: SB=0, BB=1
			if (vars->bet_actions[next][0] == 0) {
				// Player hasn't acted yet
				if (next == (vars->dealer + 1) % num_of_players) {
					next_action = "big_blind";
				}
			}
		}
		retval = verus_write_betting_state(table_id, vars, next_action);
	}
	
	return retval;
}
