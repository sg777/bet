#ifndef PLAYER_H
#define PLAYER_H

#include "bet.h"

// Player initialization states (before entering game loop)
#define P_INIT_WALLET_READY  1  // Wallet + ID verified (can read blockchain)
#define P_INIT_TABLE_FOUND   2  // Found table (have table info)
#define P_INIT_WAIT_JOIN     3  // Waiting for user approval (GUI only)
#define P_INIT_JOINING       4  // Executing payin_tx (blocking)
#define P_INIT_JOINED        5  // Successfully joined (have seat)
#define P_INIT_DECK_READY    6  // Deck loaded/initialized
#define P_INIT_IN_GAME       7  // In game loop

// Global player init state
extern int32_t player_init_state;

const char *player_init_state_str(int32_t state);
int32_t player_init_deck();
int32_t decode_card(bits256 b_blinded_card, bits256 blinded_value, cJSON *dealer_blind_info);
int32_t reveal_card(char *table_id);
int32_t handle_game_state_player(char *table_id);
int32_t handle_verus_player();

/* Write a betting action to the player's Verus identity. The dealer
 * polls P_BETTING_ACTION_KEY on this game_id and processes it. Used by
 * --cli, --auto, and --gui modes. amount is in integer table chips.
 *
 * round MUST be the current betting round number (0=preflop, 1=flop,
 * 2=turn, 3=river) as reported by the dealer in T_BETTING_STATE_KEY.
 * The dealer's verus_poll_player_action() compares this against the
 * round it expects and discards stale-round actions, so passing a
 * stale value here causes the dealer to time-out and auto-fold. */
int32_t player_write_betting_action(char *table_id, const char *action, int64_t amount, int32_t round);

/* Read the dealer's authoritative betting state for the current hand from
 * T_BETTING_STATE_KEY on the table identity. Returned cJSON is owned by the
 * caller (cJSON_Delete it). NULL if no betting state has been written yet
 * or on RPC failure. The fields the caller cares about are:
 *   "current_turn" int  - whose turn it is
 *   "round"        int  - 0=preflop, 1=flop, 2=turn, 3=river
 *   "pot"          num  - total pot in table chips
 *   "min_amount"   num  - amount needed to call (0 if can check)
 *   "turn_start_time" num - dealer's prompt timestamp (used for dedup)
 *   "player_funds" arr  - per-player remaining stacks
 */
cJSON *player_read_betting_state(char *table_id);

#endif /* PLAYER_H */
