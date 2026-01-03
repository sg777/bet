#ifndef COMMON_H
#define COMMON_H

// clang-format off
#define __FUNCTION__ __func__
#define CHANNELD_AWAITING_LOCKIN    2
#define CHANNELD_NORMAL             3

#define hand_size                   7
#define no_of_hole_cards            2
#define no_of_flop_cards            3
#define no_of_turn_card             1
#define no_of_river_card            1
#define no_of_community_cards       5

#define NSUITS 4
#define NFACES 13

#define table_stack 200


enum blinds { 
	small_blind_amount = 1, 
	big_blind_amount   = 2 
};

enum bet_dcv_state { 
	dealer_table_empty = 0, 
	dealer_table_full  = 1, 
	pos_on_table_empty = 2, 
	pos_on_table_full  = 3 
};

// Betting input mode
enum betting_mode {
	BET_MODE_AUTO = 0,   // Auto-check/call for testing
	BET_MODE_CLI  = 1,   // Read from CLI (stdin)
	BET_MODE_GUI  = 2    // Read from GUI via websocket
};

extern int g_betting_mode;

// GUI join approval mechanism
extern bool gui_join_approved;
extern pthread_mutex_t gui_join_mutex;
extern pthread_cond_t gui_join_cond;  // Global betting mode


#define mchips_msatoshichips    1000000 // 0.01mCHIPS
#define channel_fund_satoshis   16777215 // 0.167CHIPS this is the mx limit set up the c lightning node
#define satoshis                100000000 //10^8 satoshis for 1 COIN
#define satoshis_per_unit       1000
#define normalization_factor    100

#define CARDS_MAXCARDS       14 // 14 cards for 2 players (4 hole + 5 community + 5 spare)
// Note: 52-card deck requires chunked identity updates (too large for single update)
#define CARDS_MAXPLAYERS     9 // 9   //
#define CARDS_MAXROUNDS      4 // 9   //
#define CARDS_MAXCHIPS       1000
#define CARDS_CHIPSIZE       (SATOSHIDEN / CARDS_MAXCHIPS)
#define BET_PLAYERTIMEOUT       15
#define BET_GAMESTART_DELAY     10
#define BET_TURN_TIMEOUT_SECS   60      // Seconds before auto-fold
#define BET_TURN_TIMEOUT_BLOCKS 6       // Blocks before auto-fold (whichever is higher)
#define BET_RESERVERATE         1.025
#define LN_FUNDINGERROR         "\"Cannot afford funding transaction\""

#define tx_spent    0
#define tx_unspent 	1

#define DEFAULT_GUI_WS_PORT        9000  // Legacy default, use node-specific defaults below
#define DEFAULT_DEALER_WS_PORT     9000
#define DEFAULT_PLAYER_WS_PORT     9001
#define DEFAULT_CASHIER_WS_PORT    9002
extern int gui_ws_port;

// Betting amounts in CHIPS (simplified for testing)
#define default_chips_tx_fee        0.0001
#define default_bb_in_chips         0.02    // Big blind: 0.02 CHIPS
#define default_sb_in_chips         0.01    // Small blind: 0.01 CHIPS
#define default_min_stake           0.5     // Min payin: 0.5 CHIPS (25 BB)
#define default_max_stake           2.0     // Max payin: 2 CHIPS (100 BB)
// Legacy - kept for compatibility
#define default_min_stake_in_bb     25
#define default_max_stake_in_bb     100

// BET_WITHOUT_LN and BET_WITH_LN removed - Lightning Network support removed, using CHIPS-only payments      

extern bits256 v_hash[CARDS_MAXCARDS][CARDS_MAXCARDS];
extern bits256 g_hash[CARDS_MAXPLAYERS][CARDS_MAXCARDS];

extern bits256 all_v_hash[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXCARDS];
extern bits256 all_g_hash[CARDS_MAXPLAYERS][CARDS_MAXPLAYERS][CARDS_MAXCARDS];

extern struct privatebet_info *bet_dcv;
extern struct privatebet_vars *dcv_vars;

extern int32_t no_of_signers, max_no_of_signers, is_signed[CARDS_MAXPLAYERS];

extern struct privatebet_info *bet_bvv;
extern struct privatebet_vars *bvv_vars;
// bet_dcv_bvv removed - nanomsg sockets no longer used

extern struct privatebet_info *BET_player[CARDS_MAXPLAYERS];

extern int32_t all_sharesflag[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXPLAYERS];

extern int32_t all_player_card_values[CARDS_MAXPLAYERS][hand_size]; // where 7 is hand_size
extern int32_t all_number_cards_drawn[CARDS_MAXPLAYERS];
extern int32_t all_player_cards[CARDS_MAXPLAYERS][CARDS_MAXCARDS];
extern int32_t all_no_of_player_cards[CARDS_MAXPLAYERS];
extern bits256 all_playershares[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXPLAYERS];

extern int32_t permis_d[CARDS_MAXCARDS], permis_b[CARDS_MAXCARDS];
extern bits256 deckid;
extern uint8_t sharenrs[256];
extern bits256 playershares[CARDS_MAXCARDS][CARDS_MAXPLAYERS];

extern struct lws *wsi_global_client;

extern struct cashier *cashier_info;

extern int32_t max_players;

extern int32_t no_of_notaries;
extern int32_t threshold_value;

extern char **notary_node_ips;
extern char **notary_node_pubkeys;

extern double SB_in_chips;
extern double BB_in_chips;
extern double table_stake_in_chips;
extern double chips_tx_fee;
extern double table_min_stake;
extern double table_max_stake;
extern double table_stack_in_bb;

extern char dev_fund_addr[64];
extern double dev_fund_commission;
extern char *legacy_m_of_n_msig_addr;

extern int32_t no_of_txs;
extern char tx_ids[CARDS_MAXPLAYERS][100];
extern int32_t *notary_status;
extern int32_t live_notaries;

extern char table_id[65];
extern int32_t bvv_state;
extern int32_t dcv_state;

extern char dealer_ip[20];
extern char cashier_ip[20];
extern char unique_id[65];

extern double dcv_commission_percentage;
extern double max_allowed_dcv_commission;
extern char dcv_hosted_gui_url[128];

/* These are globals; define them in exactly one .c file */
extern int32_t is_table_private;
extern char table_password[128];
extern char player_name[128];
extern char verus_pid[128]; // This is the verus ID owned by the player to which player updates during the game.

// bet_ln_config removed - Lightning Network support removed, using CHIPS-only payments
extern int64_t sc_start_block;

extern char dealer_ip_for_bvv[128];
#endif
