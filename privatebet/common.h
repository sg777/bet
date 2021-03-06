#ifndef COMMON_H
#define COMMON_H

#define CHANNELD_AWAITING_LOCKIN 2
#define CHANNELD_NORMAL 3

#define hand_size 7
#define no_of_hole_cards 2
#define no_of_flop_cards 3
#define no_of_turn_card 1
#define no_of_river_card 1
#define no_of_community_cards 5

#define NSUITS 4
#define NFACES 13

#define small_blind_amount 1 // 1000000
#define big_blind_amount 2 // 2000000
#define table_stack 200

/*
#define small_blind_amount 1 // 1mCHIPS
#define big_blind_amount 2 // 2mCHIPS
*/

#define mchips_msatoshichips 1000000 // 0.01mCHIPS
#define channel_fund_satoshis 16777215 // 0.167CHIPS this is the mx limit set up the c lightning node
#define satoshis 100000000 //10^8 satoshis for 1 COIN
#define satoshis_per_unit 1000
#define normalization_factor 100

#define CARDS777_MAXCARDS 52 // 52    //
#define CARDS777_MAXPLAYERS 10 // 9   //
#define CARDS777_MAXROUNDS 4 // 9   //
#define CARDS777_MAXCHIPS 1000
#define CARDS777_CHIPSIZE (SATOSHIDEN / CARDS777_MAXCHIPS)
#define BET_PLAYERTIMEOUT 15
#define BET_GAMESTART_DELAY 10
#define BET_RESERVERATE 1.025
#define LN_FUNDINGERROR "\"Cannot afford funding transaction\""

#define tx_spent 0
#define tx_unspent 1

extern bits256 v_hash[CARDS777_MAXCARDS][CARDS777_MAXCARDS];
extern bits256 g_hash[CARDS777_MAXPLAYERS][CARDS777_MAXCARDS];

extern bits256 all_v_hash[CARDS777_MAXPLAYERS][CARDS777_MAXCARDS][CARDS777_MAXCARDS];
extern bits256 all_g_hash[CARDS777_MAXPLAYERS][CARDS777_MAXPLAYERS][CARDS777_MAXCARDS];

extern struct privatebet_info *bet_dcv;
extern struct privatebet_vars *dcv_vars;

extern int32_t no_of_signers, max_no_of_signers, is_signed[CARDS777_MAXPLAYERS];

extern struct privatebet_info *bet_bvv;
extern struct privatebet_vars *bvv_vars;

extern struct privatebet_info *BET_player[CARDS777_MAXPLAYERS];

extern int32_t all_sharesflag[CARDS777_MAXPLAYERS][CARDS777_MAXCARDS][CARDS777_MAXPLAYERS];

extern int32_t all_player_card_values[CARDS777_MAXPLAYERS][hand_size]; // where 7 is hand_size
extern int32_t all_number_cards_drawn[CARDS777_MAXPLAYERS];
extern int32_t all_player_cards[CARDS777_MAXPLAYERS][CARDS777_MAXCARDS];
extern int32_t all_no_of_player_cards[CARDS777_MAXPLAYERS];
extern bits256 all_playershares[CARDS777_MAXPLAYERS][CARDS777_MAXCARDS][CARDS777_MAXPLAYERS];

extern int32_t permis_d[CARDS777_MAXCARDS], permis_b[CARDS777_MAXCARDS];
extern bits256 deckid;
extern uint8_t sharenrs[256];
extern bits256 playershares[CARDS777_MAXCARDS][CARDS777_MAXPLAYERS];

extern struct lws *wsi_global_client;

extern struct cashier *cashier_info;

extern int32_t max_players;

extern int32_t no_of_notaries;
extern int32_t threshold_value;

extern char **notary_node_ips;
extern char **notary_node_pubkeys;

extern double table_stack_in_chips;
extern double chips_tx_fee;

extern char dev_fund_addr[64];
extern char *legacy_m_of_n_msig_addr;

extern int32_t no_of_txs;
extern char tx_ids[CARDS777_MAXPLAYERS][100];
extern int32_t *notary_status;
extern int32_t live_notaries;

extern char table_id[65];
extern int32_t bvv_state;
extern int32_t dcv_state;

extern char dealer_ip[20];
extern char cashier_ip[20];
extern char unique_id[65];

#endif
