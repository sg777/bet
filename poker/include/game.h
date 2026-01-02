#ifndef GAME_H
#define GAME_H

#include "bet.h"

#define G_ZEROIZED_STATE 0
#define G_TABLE_ACTIVE 1
#define G_TABLE_STARTED 2
#define G_PLAYERS_JOINED 3
#define G_DECK_SHUFFLING_P 4
#define G_DECK_SHUFFLING_D 5
#define G_DECK_SHUFFLING_B 6
#define G_REVEAL_CARD 7
#define G_REVEAL_CARD_P_DONE 8
#define G_ROUND_BETTING 9
#define G_SHOWDOWN 10
#define G_SETTLEMENT_PENDING 11
#define G_SETTLEMENT_COMPLETE 12

struct t_game_info_struct {
	int32_t game_state;
	cJSON *game_info;
};

extern struct t_game_info_struct t_game_info;

// Global start_block for reading combined CMM view
// Set by dealer during init, by player when joining table
extern int32_t g_start_block;

const char *game_state_str(int32_t game_state);
cJSON *append_game_state(char *table_id, int32_t game_state, cJSON *game_state_info);
int32_t get_game_state(char *table_id);
cJSON *get_game_state_info(char *table_id);
int32_t init_game_state(char *table_id);
int32_t is_card_drawn(char *table_id);
int32_t verus_receive_card(char *table_id, struct privatebet_vars *vars);
int32_t verus_small_blind(char *table_id, struct privatebet_vars *vars);

// Round betting - dealer writes state, polls player actions
int32_t verus_write_betting_state(char *table_id, struct privatebet_vars *vars, const char *action);
cJSON *verus_poll_player_action(char *table_id, int32_t player_idx, int32_t expected_round);
int32_t verus_process_betting_action(char *table_id, struct privatebet_vars *vars, cJSON *action_info);
int32_t verus_next_turn(struct privatebet_vars *vars);
int32_t verus_handle_round_betting(char *table_id, struct privatebet_vars *vars);

// Board cards (community cards) - dealer polls players and updates table ID
int32_t update_board_cards(char *table_id, int32_t card_type);

#endif /* GAME_H */
