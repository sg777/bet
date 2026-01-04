#ifndef GUI_H
#define GUI_H

#include "../../includes/cJSON.h"

// Card value to string conversion (0-51 -> "2c", "Ah", etc.)
const char *card_value_to_string(int32_t card_value);

// Convert array of card values to JSON array of strings
cJSON *cards_to_json_array(int32_t *cards, int32_t count);

// Send a message to the GUI via WebSocket
void gui_send_message(cJSON *message);

// Send player initialization state to GUI
void send_init_state_to_gui(int32_t state);

// GUI Message builders
cJSON *gui_build_backend_status(int32_t status);
cJSON *gui_build_wallet_info(double balance, const char *addr, double table_min_stake, double small_blind, double big_blind, int32_t max_players);
cJSON *gui_build_seats_info(int32_t num_players, double *chips, int32_t *connected);
cJSON *gui_build_blinds_info(double small_blind, double big_blind);
cJSON *gui_build_dealer_info(int32_t dealer_seat);
cJSON *gui_build_deal_holecards(int32_t card1, int32_t card2, double balance);
cJSON *gui_build_deal_board(int32_t *board_cards, int32_t count);
cJSON *gui_build_betting_round(int32_t playerid, double pot, double to_call, double min_raise, 
                               int32_t *possibilities, int32_t poss_count, double *player_funds, int32_t num_players);
cJSON *gui_build_betting_action(int32_t playerid, const char *action, double bet_amount);
cJSON *gui_build_final_info(int32_t *winners, int32_t winner_count, double win_amount,
                            int32_t **all_holecards, int32_t *board_cards);

#endif /* GUI_H */

