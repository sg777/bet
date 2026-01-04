/******************************************************************************
 * GUI Communication Module
 * 
 * Handles sending messages to the React GUI via WebSocket.
 * Messages follow the format expected by pangea-poker GUI.
 ******************************************************************************/

#include "gui.h"
#include "bet.h"
#include "common.h"
#include "client.h"
#include "macrologger.h"
#include <string.h>

// Card suit and rank characters
static const char SUITS[] = "cdhs";  // clubs, diamonds, hearts, spades
static const char RANKS[] = "23456789TJQKA";

// Static buffer for card strings (not thread-safe, but fine for single-threaded use)
static char card_str_buffer[4];

const char *card_value_to_string(int32_t card_value)
{
    if (card_value < 0 || card_value > 51) {
        return "??";
    }
    
    int32_t suit = card_value / 13;
    int32_t rank = card_value % 13;
    
    card_str_buffer[0] = RANKS[rank];
    card_str_buffer[1] = SUITS[suit];
    card_str_buffer[2] = '\0';
    
    return card_str_buffer;
}

cJSON *cards_to_json_array(int32_t *cards, int32_t count)
{
    cJSON *arr = cJSON_CreateArray();
    for (int32_t i = 0; i < count; i++) {
        if (cards[i] >= 0 && cards[i] <= 51) {
            // Need to create a copy since card_value_to_string uses static buffer
            char card_copy[4];
            strncpy(card_copy, card_value_to_string(cards[i]), sizeof(card_copy));
            cJSON_AddItemToArray(arr, cJSON_CreateString(card_copy));
        }
    }
    return arr;
}

void gui_send_message(cJSON *message)
{
    extern int g_betting_mode;
    
    if (message == NULL) {
        dlg_warn("Attempted to send NULL message to GUI");
        return;
    }
    
    // Always log the message for debugging/testing
    char *msg_str = cJSON_PrintUnformatted(message);
    if (msg_str) {
        dlg_info("[GUI] %s", msg_str);
        free(msg_str);
    }
    
    // Only send via WebSocket in GUI mode
    if (g_betting_mode == BET_MODE_GUI) {
        extern void player_lws_write(cJSON *data);
        player_lws_write(message);
    }
}

cJSON *gui_build_backend_status(int32_t status)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "backend_status");
    cJSON_AddNumberToObject(msg, "backend_status", status);
    return msg;
}

cJSON *gui_build_wallet_info(double balance, const char *addr, double table_min_stake, double small_blind, double big_blind, int32_t max_players)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "walletInfo");
    cJSON_AddNumberToObject(msg, "backend_status", 1);
    cJSON_AddNumberToObject(msg, "balance", balance);
    if (addr) {
        cJSON_AddStringToObject(msg, "addr", addr);
    }
    cJSON_AddNumberToObject(msg, "table_min_stake", table_min_stake);
    cJSON_AddNumberToObject(msg, "small_blind", small_blind);
    cJSON_AddNumberToObject(msg, "big_blind", big_blind);
    cJSON_AddNumberToObject(msg, "max_players", max_players);
    return msg;
}

cJSON *gui_build_seats_info(int32_t num_players, double *chips, int32_t *connected)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "seats");
    
    cJSON *seats = cJSON_CreateArray();
    for (int32_t i = 0; i < num_players; i++) {
        cJSON *seat = cJSON_CreateObject();
        char name[32];
        snprintf(name, sizeof(name), "Player%d", i + 1);
        cJSON_AddStringToObject(seat, "name", name);
        cJSON_AddNumberToObject(seat, "playing", 0);
        cJSON_AddNumberToObject(seat, "seat", i);
        cJSON_AddNumberToObject(seat, "chips", chips ? chips[i] : 0);
        cJSON_AddBoolToObject(seat, "empty", connected ? !connected[i] : true);
        cJSON_AddItemToArray(seats, seat);
    }
    cJSON_AddItemToObject(msg, "seats", seats);
    return msg;
}

cJSON *gui_build_blinds_info(double small_blind, double big_blind)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "blindsInfo");
    cJSON_AddNumberToObject(msg, "small_blind", small_blind);
    cJSON_AddNumberToObject(msg, "big_blind", big_blind);
    return msg;
}

cJSON *gui_build_dealer_info(int32_t dealer_seat)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "dealer");
    cJSON_AddNumberToObject(msg, "playerid", dealer_seat);
    return msg;
}

cJSON *gui_build_deal_holecards(int32_t card1, int32_t card2, double balance)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "deal");
    
    cJSON *deal = cJSON_CreateObject();
    cJSON *holecards = cJSON_CreateArray();
    
    // Convert card values to strings
    char c1[4], c2[4];
    strncpy(c1, card_value_to_string(card1), sizeof(c1));
    strncpy(c2, card_value_to_string(card2), sizeof(c2));
    
    cJSON_AddItemToArray(holecards, cJSON_CreateString(c1));
    cJSON_AddItemToArray(holecards, cJSON_CreateString(c2));
    
    cJSON_AddItemToObject(deal, "holecards", holecards);
    cJSON_AddNumberToObject(deal, "balance", balance);
    
    cJSON_AddItemToObject(msg, "deal", deal);
    return msg;
}

cJSON *gui_build_deal_board(int32_t *board_cards, int32_t count)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "deal");
    
    cJSON *deal = cJSON_CreateObject();
    cJSON *board = cJSON_CreateArray();
    
    for (int32_t i = 0; i < count && i < 5; i++) {
        if (board_cards[i] >= 0) {
            char card[4];
            strncpy(card, card_value_to_string(board_cards[i]), sizeof(card));
            cJSON_AddItemToArray(board, cJSON_CreateString(card));
        }
    }
    
    cJSON_AddItemToObject(deal, "board", board);
    cJSON_AddItemToObject(msg, "deal", deal);
    return msg;
}

cJSON *gui_build_betting_round(int32_t playerid, double pot, double to_call, double min_raise,
                               int32_t *possibilities, int32_t poss_count, double *player_funds, int32_t num_players)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "betting");
    cJSON_AddStringToObject(msg, "action", "round_betting");
    cJSON_AddNumberToObject(msg, "playerid", playerid);
    cJSON_AddNumberToObject(msg, "pot", pot);
    cJSON_AddNumberToObject(msg, "toCall", to_call);
    cJSON_AddNumberToObject(msg, "minRaiseTo", min_raise);
    
    // Possibilities array
    if (possibilities && poss_count > 0) {
        cJSON *poss = cJSON_CreateArray();
        for (int32_t i = 0; i < poss_count; i++) {
            cJSON_AddItemToArray(poss, cJSON_CreateNumber(possibilities[i]));
        }
        cJSON_AddItemToObject(msg, "possibilities", poss);
    }
    
    // Player funds array
    if (player_funds && num_players > 0) {
        cJSON *funds = cJSON_CreateArray();
        for (int32_t i = 0; i < num_players; i++) {
            cJSON_AddItemToArray(funds, cJSON_CreateNumber(player_funds[i]));
        }
        cJSON_AddItemToObject(msg, "player_funds", funds);
    }
    
    return msg;
}

cJSON *gui_build_betting_action(int32_t playerid, const char *action, double bet_amount)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "betting");
    cJSON_AddStringToObject(msg, "action", action);
    cJSON_AddNumberToObject(msg, "playerid", playerid);
    cJSON_AddNumberToObject(msg, "bet_amount", bet_amount);
    return msg;
}

cJSON *gui_build_final_info(int32_t *winners, int32_t winner_count, double win_amount,
                            int32_t **all_holecards, int32_t *board_cards)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "method", "finalInfo");
    
    // Winners array
    cJSON *winners_arr = cJSON_CreateArray();
    for (int32_t i = 0; i < winner_count; i++) {
        cJSON_AddItemToArray(winners_arr, cJSON_CreateNumber(winners[i]));
    }
    cJSON_AddItemToObject(msg, "winners", winners_arr);
    cJSON_AddNumberToObject(msg, "win_amount", win_amount);
    
    // Show info
    cJSON *show_info = cJSON_CreateObject();
    
    // All hole cards info
    if (all_holecards) {
        cJSON *all_holes = cJSON_CreateArray();
        // Assuming 2 players with 2 cards each for now
        for (int32_t p = 0; p < 2; p++) {
            cJSON *player_cards = cJSON_CreateArray();
            if (all_holecards[p]) {
                for (int32_t c = 0; c < 2; c++) {
                    if (all_holecards[p][c] >= 0) {
                        char card[4];
                        strncpy(card, card_value_to_string(all_holecards[p][c]), sizeof(card));
                        cJSON_AddItemToArray(player_cards, cJSON_CreateString(card));
                    }
                }
            }
            cJSON_AddItemToArray(all_holes, player_cards);
        }
        cJSON_AddItemToObject(show_info, "allHoleCardsInfo", all_holes);
    }
    
    // Board cards
    if (board_cards) {
        cJSON *board = cJSON_CreateArray();
        for (int32_t i = 0; i < 5; i++) {
            if (board_cards[i] >= 0) {
                char card[4];
                strncpy(card, card_value_to_string(board_cards[i]), sizeof(card));
                cJSON_AddItemToArray(board, cJSON_CreateString(card));
            }
        }
        cJSON_AddItemToObject(show_info, "boardCardInfo", board);
    }
    
    cJSON_AddItemToObject(msg, "showInfo", show_info);
    return msg;
}

