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

#endif /* PLAYER_H */
