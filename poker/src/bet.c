/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
#include "bet.h"
#include "../../includes/curl/curl.h"
#include "macrologger.h"
#include "cards.h"
#include "cashier.h"
#include "client.h"
#include "commands.h"
#include "gfshare.h"
#include "host.h"
#include "network.h"
#include "table.h"
#include "storage.h"
#include "config.h"
#include "heartbeat.h"
#include "misc.h"
#include "help.h"
#include "err.h"
#include "switchs.h"
#include "player.h"
#include "print.h"
#include "test.h"
#include "dealer.h"
#include "blinder.h"
#include "dealer_registration.h"

#include <string.h>
#include <strings.h>

//#define LIVE_THREAD 0

/* Bet without LN completed development after this block, so only the tx's which generated 
   after this block contain the info of the games played using bet without ln setup.
*/
int64_t sc_start_block = 9693174;

struct privatebet_info *bet_player = NULL;
struct privatebet_vars *player_vars = NULL;

uint8_t sharenrs[256];
bits256 deckid;
bits256 playershares[CARDS_MAXCARDS][CARDS_MAXPLAYERS];
int32_t permis_d[CARDS_MAXCARDS], permis_b[CARDS_MAXCARDS];
bits256 v_hash[CARDS_MAXCARDS][CARDS_MAXCARDS];
bits256 g_hash[CARDS_MAXPLAYERS][CARDS_MAXCARDS];
struct enc_share *g_shares = NULL;

char dealer_ip[20];
char unique_id[65];

struct seat_info player_seats_info[CARDS_MAXPLAYERS];

int32_t player_pos[CARDS_MAXPLAYERS];

/**************************************************************************************************
This value is read from dealer_config.json file, it defines the exact number of players that needs
be joined in order to play the game.
The default value is 2, i.e as atleast two players are required to play the game.
***************************************************************************************************/

int32_t max_players = 2;

static const int32_t poker_deck_size = 52;
int32_t bet_node_type;

static void bet_cashier_deinitialize()
{
	if (cashier_info)
		free(cashier_info);
}

static void bet_player_initialize(char *dcv_ip)
{
	// Nanomsg sockets removed - no longer used
	player_vars = calloc(1, sizeof(struct privatebet_vars));

	bet_player = calloc(1, sizeof(struct privatebet_info));
	bet_player->maxplayers = (max_players < CARDS_MAXPLAYERS) ? max_players : CARDS_MAXPLAYERS;
	bet_player->maxchips = CARDS_MAXCHIPS;
	bet_player->chipsize = CARDS_CHIPSIZE;
	bet_player->numplayers = max_players;
	bet_info_set(bet_player, "demo", poker_deck_size, 0, max_players);
}

static void bet_player_deinitialize()
{
	if (bet_player)
		free(bet_player);
	if (player_vars)
		free(player_vars);
}

static void bet_player_thrd(char *dcv_ip)
{
	pthread_t player_thrd, player_backend_write, player_backend_read;

#if 0
	bet_player_initialize(dcv_ip);
#endif
	if (OS_thread_create(&player_thrd, NULL, (void *)bet_player_backend_loop, (void *)bet_player) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
	if (OS_thread_create(&player_backend_read, NULL, (void *)bet_player_frontend_read_loop, NULL) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
	if (OS_thread_create(&player_backend_write, NULL, (void *)bet_player_frontend_write_loop, NULL) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}

	if (pthread_join(player_backend_read, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	if (pthread_join(player_backend_write, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	if (pthread_join(player_thrd, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}

	bet_player_deinitialize();
}

static void bet_bvv_initialize(char *dcv_ip, const int32_t port)
{
	// Nanomsg sockets removed - no longer used
	bvv_vars = calloc(1, sizeof(struct privatebet_vars));
	bet_bvv = calloc(1, sizeof(struct privatebet_info));
	bet_bvv->maxplayers = (max_players < CARDS_MAXPLAYERS) ? max_players : CARDS_MAXPLAYERS;
	bet_bvv->maxchips = CARDS_MAXCHIPS;
	bet_bvv->chipsize = CARDS_CHIPSIZE;
	bet_bvv->numplayers = max_players;
	bet_bvv->myplayerid = -1;
	bet_info_set(bet_bvv, "demo", poker_deck_size, 0, max_players);
}

static void bet_bvv_deinitialize()
{
	if (bet_bvv)
		free(bet_bvv);
	if (bvv_vars)
		free(bvv_vars);
}

void bet_bvv_thrd(char *dcv_ip, const int32_t port)
{
	pthread_t bvv_backend /*, bvv_frontend*/;

	bet_bvv_initialize(dcv_ip, port);
	if (OS_thread_create(&bvv_backend, NULL, (void *)bet_bvv_backend_loop, (void *)bet_bvv) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
	if (pthread_join(bvv_backend, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	bet_bvv_deinitialize();
}

// bet_dcv_bvv_initialize removed - dcv_bvv_sock_info struct removed, nanomsg no longer used

static int32_t bet_dcv_initialize(char *dcv_ip)
{
	int32_t retval = OK;
	// Nanomsg sockets removed - no longer used
	bet_dcv = calloc(1, sizeof(struct privatebet_info));
	bet_dcv->maxplayers = (max_players < CARDS_MAXPLAYERS) ? max_players : CARDS_MAXPLAYERS;
	bet_dcv->maxchips = CARDS_MAXCHIPS;
	bet_dcv->chipsize = CARDS_CHIPSIZE;
	bet_dcv->numplayers = 0;
	bet_dcv->myplayerid = -2;
	bet_dcv->cardid = -1;
	bet_dcv->turni = -1;
	bet_dcv->no_of_turns = 0;
	bet_info_set(bet_dcv, "demo", poker_deck_size, 0, max_players);
	bet_dcv->msg = cJSON_CreateObject();

	dcv_vars = calloc(1, sizeof(struct privatebet_vars));

	dcv_vars->turni = 0;
	dcv_vars->round = 0;
	dcv_vars->pot = 0;
	dcv_vars->last_turn = 0;
	dcv_vars->last_raise = 0;
	for (int i = 0; i < CARDS_MAXPLAYERS; i++) {
		dcv_vars->funds[i] = 0;
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			dcv_vars->bet_actions[i][j] = 0;
			dcv_vars->betamount[i][j] = 0;
		}
	}

	for (int32_t i = 0; i < CARDS_MAXPLAYERS; i++) {
		player_pos[i] = 0;
	}
	return retval;
}

static void bet_dcv_deinitialize()
{
	if (bet_dcv)
		free(bet_dcv);
	if (dcv_vars)
		free(dcv_vars);
}

static void bet_dcv_thrd(char *dcv_ip)
{
	int32_t retval = OK;
	pthread_t dcv_backend, dcv_thrd;

#ifdef LIVE_THREAD
	pthread_t live_thrd;
#endif

	retval = bet_dcv_initialize(dcv_ip);
	if (retval != OK) {
		dlg_error("%s", bet_err_str(retval));
		exit(-1);
	}
	// bet_dcv_bvv_initialize removed - dcv_bvv_sock_info struct removed

#ifdef LIVE_THREAD
	if (OS_thread_create(&live_thrd, NULL, (void *)bet_dcv_heartbeat_loop, (void *)bet_dcv) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
#endif
	if (OS_thread_create(&dcv_backend, NULL, (void *)bet_dcv_backend_loop, (void *)bet_dcv) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
	// bet_dcv_bvv_backend_loop removed - dcv_bvv_sock_info struct removed
	if (OS_thread_create(&dcv_thrd, NULL, (void *)bet_dcv_frontend_loop, NULL) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}

	if (pthread_join(dcv_backend, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	if (pthread_join(dcv_thrd, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	// dcv_bvv_thrd removed - bet_dcv_bvv_backend_loop removed
#ifdef LIVE_THREAD
	if (pthread_join(live_thrd, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
#endif
	bet_dcv_deinitialize();
}

static void bet_cashier_server_initialize(char *node_ip)
{
	// Nanomsg sockets removed - no longer used
	cashier_info = calloc(1, sizeof(struct cashier));
}

static void bet_cashier_server_thrd(char *node_ip)
{
	pthread_t server_thrd;

	bet_cashier_server_initialize(node_ip);
	update_cashiers(node_ip);
	if (OS_thread_create(&server_thrd, NULL, (void *)bet_cashier_server_loop, (void *)cashier_info) != 0) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_LAUNCHING));
		exit(-1);
	}
	if (pthread_join(server_thrd, NULL)) {
		dlg_error("%s", bet_err_str(ERR_PTHREAD_JOINING));
	}
	bet_cashier_deinitialize();
}

static void bet_set_unique_id()
{
	bits256 randval;
	memset(unique_id, 0x00, sizeof(unique_id));
	OS_randombytes(randval.bytes, sizeof(randval));
	bits256_str(unique_id, randval);
}

static void common_init()
{
	OS_init();
	libgfshare_init();
	// Lightning Network support removed - no longer used
	bet_sqlite3_init();
	bet_parse_cashier_config_ini_file();
}

static void cashier_init()
{
	common_init();
	bet_clear_tables();
}

static void playing_nodes_init()
{
	common_init();
#if 0
	bet_check_cashier_nodes();
#endif
	bet_parse_player_config_ini_file();
}

static void dealer_node_init()
{
	if (0 == strlen(dcv_hosted_gui_url)) {
		sprintf(dcv_hosted_gui_url, "http://%s:1234/", dealer_ip);
	}
	dlg_warn("Delaer GUI URL :: %s", dcv_hosted_gui_url); // dlg_warn is just to highlight the log in the console
	if (0 == check_url(dcv_hosted_gui_url))
		memset(dcv_hosted_gui_url, 0x00, sizeof(dcv_hosted_gui_url));

	bet_set_table_id();
#if 0
	bet_compute_m_of_n_msig_addr();
	bet_game_multisigaddress();
#endif
	bet_init_player_seats_info();
}

static void bet_send_dealer_info_to_cashier(char *dealer_ip)
{
	cJSON *dealer_info = NULL;

	dealer_info = cJSON_CreateObject();
	cJSON_AddStringToObject(dealer_info, "method", "dealer_info");
	cJSON_AddStringToObject(dealer_info, "ip", dealer_ip);

	for (int32_t i = 0; i < no_of_notaries; i++) {
		if (notary_status[i] == 1) {
			bet_msg_cashier(dealer_info, notary_node_ips[i]);
		}
	}
}

static char *bet_pick_dealer()
{
	cJSON *available_dealers = NULL;

	available_dealers = bet_get_available_dealers();
	dlg_info("Available dealers :: %s", cJSON_Print(available_dealers));

	for (int32_t i = 0; i < cJSON_GetArraySize(available_dealers); i++) {
		cJSON *temp = cJSON_GetArrayItem(available_dealers, i);
		if (jint(temp, "dcv_state") == dealer_table_empty) {
			return jstr(temp, "ip");
		}
	}
	return NULL;
}

void bet_start(int argc, char **argv)
{
	int32_t retval = OK;
	const char *cmd = (argc < 2) ? NULL : argv[1];

	// No command provided - show help
	if (argc < 2) {
		bet_command_info();
		return;
	}

	// Handle newblock command (early return, no initialization needed)
	if (strcmp(cmd, "newblock") == 0 && argc == 3) {
		process_block(argv[2]);
		return;
	}

	// Initialize common components for other commands
	bet_init_config_paths(); // Initialize config paths relative to executable
	bet_set_unique_id();
	bet_parse_blockchain_config_ini_file();

	// Dealer management commands
	if (strcmp(cmd, "add_dealer") == 0 && argc == 3) {
		retval = add_dealer(argv[2]);
	} else if (strcmp(cmd, "register_dealer") == 0 && argc == 3) {
		retval = register_dealer(argv[2]);
	} else if (strcmp(cmd, "deregister_dealer") == 0 && argc == 3) {
		retval = deregister_dealer(argv[2]);
	} else if (strcmp(cmd, "raise_registration_dispute") == 0) {
		if (argc != 4) {
			dlg_error("Usage: %s raise_registration_dispute <dealer_id> <action>", argv[0]);
			return;
		}
		raise_dealer_registration_dispute(argv[2], argv[3]);
	}
	// Node type commands
	else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "cashier") == 0) {
		pthread_t cashier_frontend_thrd;
		dlg_info("Starting cashier node");
		bet_node_type = cashier;
		
		// Start GUI backend thread for cashier FIRST - it must run regardless of backend errors
		if (OS_thread_create(&cashier_frontend_thrd, NULL, (void *)bet_cashier_frontend_loop, NULL) != 0) {
			dlg_error("[GUI] Failed to start GUI thread: %s", bet_err_str(ERR_PTHREAD_LAUNCHING));
			retval = ERR_PTHREAD_LAUNCHING;
		} else {
			dlg_info("[GUI] GUI backend thread started for cashier on port %d", gui_ws_port);
			// Detach the thread so it runs in background independently
			pthread_detach(cashier_frontend_thrd);
			// Give GUI thread a moment to start
			sleep(1);
		}
		
		// Start backend initialization - GUI will continue even if this fails
		if (retval == OK) {
			cashier_game_init("sg777_t");
			dlg_info("[GUI] GUI thread continues running on port %d", gui_ws_port);
			// Keep process alive so GUI thread continues
			while (1) {
				sleep(60);
			}
		}
	} else if (strcmp(cmd, "d") == 0 || strcmp(cmd, "dcv") == 0 || strcmp(cmd, "dealer") == 0) {
		pthread_t dcv_frontend_thrd;
		bet_node_type = dealer;
		
		// Start GUI backend thread for dealer FIRST - it must run regardless of backend errors
		if (OS_thread_create(&dcv_frontend_thrd, NULL, (void *)bet_dcv_frontend_loop, NULL) != 0) {
			dlg_error("[GUI] Failed to start GUI thread: %s", bet_err_str(ERR_PTHREAD_LAUNCHING));
			retval = ERR_PTHREAD_LAUNCHING;
		} else {
			dlg_info("[GUI] GUI backend thread started for dealer on port %d", gui_ws_port);
			// Detach the thread so it runs in background independently
			pthread_detach(dcv_frontend_thrd);
			// Give GUI thread a moment to start
			sleep(1);
		}
		
		// Start backend initialization - GUI will continue even if this fails
		if (retval == OK) {
			retval = bet_parse_verus_dealer();
			if (retval != OK) {
				dlg_warn("[Backend] Backend initialization failed: %s", bet_err_str(retval));
				dlg_info("[GUI] GUI thread continues running on port %d despite backend error", gui_ws_port);
				// Keep process alive so GUI thread continues
				while (1) {
					sleep(60);
				}
			}
		}
	} else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "player") == 0) {
		pthread_t player_frontend_read_thrd, player_frontend_write_thrd;
		bet_node_type = player;
		
		// Start GUI backend threads for player FIRST - they must run regardless of backend errors
		if (OS_thread_create(&player_frontend_read_thrd, NULL, (void *)bet_player_frontend_read_loop, NULL) != 0) {
			dlg_error("[GUI] Failed to start player read thread: %s", bet_err_str(ERR_PTHREAD_LAUNCHING));
			retval = ERR_PTHREAD_LAUNCHING;
		} else if (OS_thread_create(&player_frontend_write_thrd, NULL, (void *)bet_player_frontend_write_loop, NULL) != 0) {
			dlg_error("[GUI] Failed to start player write thread: %s", bet_err_str(ERR_PTHREAD_LAUNCHING));
			retval = ERR_PTHREAD_LAUNCHING;
		} else {
			dlg_info("[GUI] GUI backend threads started for player on ports %d and 9001", gui_ws_port);
			// Detach the threads so they run in background independently
			pthread_detach(player_frontend_read_thrd);
			pthread_detach(player_frontend_write_thrd);
			// Give GUI threads a moment to start
			sleep(1);
		}
		
		// Start backend initialization - GUI will continue even if this fails
		if (retval == OK) {
			retval = handle_verus_player();
			if (retval != OK) {
				dlg_warn("[Backend] Backend initialization failed: %s", bet_err_str(retval));
				dlg_info("[GUI] GUI threads continue running on ports %d and 9001 despite backend error", gui_ws_port);
				// Keep process alive so GUI threads continue
				while (1) {
					sleep(60);
				}
			}
		}
	}
	// Game commands
	else if (strcmp(cmd, "game") == 0) {
		playing_nodes_init();
		bet_handle_game(argc, argv);
	} else if (strcmp(cmd, "list_dealers") == 0) {
		cJSON *dealers = list_dealers();
		if (dealers) {
			dlg_info("Dealers ::%s", cJSON_Print(dealers));
		}
	} else if (strcmp(cmd, "list_tables") == 0) {
		list_tables();
	}
	// Transaction commands
	else if (strcmp(cmd, "consolidate") == 0) {
		cJSON *tx = NULL;
		double amount = chips_get_balance() - chips_tx_fee;
		tx = chips_transfer_funds(amount, chips_get_new_address());
		if (tx) {
			dlg_info("Consolidated tx::%s", cJSON_Print(tx));
		}
	} else if (strcmp(cmd, "withdraw") == 0) {
		if (argc == 4) {
			cJSON *tx = NULL;
			double amount = (strcmp(argv[2], "all") == 0) 
				? chips_get_balance() - chips_tx_fee 
				: atof(argv[2]);
			tx = chips_transfer_funds(amount, argv[3]);
			if (tx) {
				dlg_info("tx details::%s", cJSON_Print(tx));
			}
		} else {
			bet_help_withdraw_command_usage();
		}
	} else if (strcmp(cmd, "tx_split") == 0 && argc == 4) {
		do_split_tx_amount(atof(argv[2]), atoi(argv[3]));
	} else if (strcmp(cmd, "spendable") == 0) {
		cJSON *spendable_tx = chips_spendable_tx();
		dlg_info("CHIPS Spendable tx's :: %s\n", cJSON_Print(spendable_tx));
	}
	// Data extraction and scanning
	else if (strcmp(cmd, "extract_tx_data") == 0) {
		if (argc == 3) {
			cJSON *temp = chips_extract_tx_data_in_JSON(argv[2]);
			if (temp) {
				dlg_info("%s", cJSON_Print(temp));
			}
		} else {
			bet_help_extract_tx_data_command_usage();
		}
	} else if (strcmp(cmd, "scan") == 0) {
		bet_sqlite3_init();
		scan_games_info();
	}
	// Print/display commands
	else if (strcmp(cmd, "print") == 0 && argc > 3) {
		print_vdxf_info(argc, argv);
	} else if (strcmp(cmd, "print_id") == 0 && argc > 3) {
		print_id_info(argc, argv);
	} else if (strcmp(cmd, "print_table_key") == 0 && argc >= 3) {
		print_table_key_info(argc, argv);
	}
	// Identity management
	else if (strcmp(cmd, "reset_id") == 0 && argc == 3) {
		if (id_cansignfor(argv[2], 0, &retval)) {
			cJSON *out = update_cmm(argv[2], NULL);
			dlg_info("%s", cJSON_Print(out));
		}
	}
	// Help commands
	else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "-h") == 0 || 
		 strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0) {
		if (argc == 3) {
			bet_help_command(argv[2]);
		} else {
			bet_command_info();
		}
	}
	// Unknown command
	else {
		bet_command_info();
	}

end:
	if (retval != OK) {
		dlg_info("Error ::%s", bet_err_str(retval));
	}
}

// test_x() removed - unused function

int main(int argc, char **argv)
{
	bet_start(argc, argv);
	return 0;
}

bits256 curve25519_fieldelement(bits256 hash)
{
	hash.bytes[0] &= 0xf8, hash.bytes[31] &= 0x7f, hash.bytes[31] |= 0x40;
	return (hash);
}

bits256 card_rand256(int32_t privkeyflag, int8_t index)
{
	bits256 randval;
	OS_randombytes(randval.bytes, sizeof(randval));
	if (privkeyflag != 0)
		randval.bytes[0] &= 0xf8, randval.bytes[31] &= 0x7f, randval.bytes[31] |= 0x40;
	randval.bytes[30] = index;
	return (randval);
}

struct pair256 deckgen_common(struct pair256 *randcards, int32_t numcards)
{
	int32_t i;
	struct pair256 key, tmp;
	key.priv = curve25519_keypair(&key.prod);
	for (i = 0; i < numcards; i++) {
		tmp.priv = card_rand256(1, i);
		tmp.prod = curve25519(tmp.priv, curve25519_basepoint9());
		randcards[i] = tmp;
	}
	return (key);
}

void deckgen_common2(struct pair256 *randcards, int32_t numcards)
{
	for (int32_t i = 0; i < numcards; i++)
		randcards[i].priv = curve25519_keypair(&randcards[i].prod);
}

struct pair256 deckgen_player(bits256 *playerprivs, bits256 *playercards, int32_t *permis, int32_t numcards)
{
	int32_t i;
	struct pair256 key, randcards[256];
	char hexstr[65];

	key = deckgen_common(randcards, numcards);
	bet_permutation(permis, numcards);
	dlg_info("The player key values");
	dlg_info("priv key::%s", bits256_str(hexstr, key.priv));
	dlg_info("pub key::%s", bits256_str(hexstr, key.prod));

	//dlg_info("The player private key card values");
	for (i = 0; i < numcards; i++) {
		playerprivs[i] = randcards[i].priv; // permis[i]
		playercards[i] = curve25519(playerprivs[i], key.prod);
		//dlg_info("card ::%d::%s",i,bits256_str(hexstr,playercards[i]));
	}
	return (key);
}

int32_t deckgen_vendor(int32_t playerid, bits256 *cardprods, bits256 *finalcards, int32_t numcards,
			     bits256 *playercards,
			     bits256 deckid) // given playercards[], returns cardprods[] and finalcards[]
{
	int32_t retval = OK;
	static struct pair256 randcards[256];
	static bits256 hash_temp[CARDS_MAXCARDS];
	bits256 hash, xoverz, tmp[256];

	deckgen_common2(randcards, numcards);

	for (int32_t i = 0; i < numcards; i++) {
		xoverz = xoverz_donna(curve25519(randcards[i].priv, playercards[i]));
		vcalc_sha256(0, hash.bytes, xoverz.bytes, sizeof(xoverz));
		hash_temp[i] = hash; // optimization
		tmp[i] = fmul_donna(curve25519_fieldelement(hash), randcards[i].priv);
	}

	for (int32_t i = 0; i < numcards; i++) {
		finalcards[i] = tmp[permis_d[i]]; //sg777 tmp[permis_d[i]]
		g_hash[playerid][i] = hash_temp[permis_d[i]]; // sg777 hash_temp[permis_d[i]]
		cardprods[i] = randcards[i].prod; // same cardprods[] returned for each player
	}
	return retval;
}

struct pair256 p2p_bvv_init(bits256 *keys, struct pair256 b_key, bits256 *blindings, bits256 *blindedcards,
			    bits256 *finalcards, int32_t numcards, int32_t numplayers, int32_t playerid, bits256 deckid)
{
	int32_t i, j, M;
	uint8_t space[8192];
	bits256 cardshares[CARDS_MAXPLAYERS];
	struct enc_share temp;

	for (i = 0; i < numcards; i++) {
		blindings[i] = rand256(1);
		blindedcards[i] = fmul_donna(finalcards[permis_b[i]],
					     blindings[i]); //sg777 fmul_donna(finalcards[i], blindings[i])
	}

	M = (numplayers / 2) + 1;

	gfshare_calc_sharenrs(sharenrs, numplayers, deckid.bytes,
			      sizeof(deckid)); // same for all players for this round

	for (i = 0; i < numcards; i++) {
		gfshare_calc_shares(cardshares[0].bytes, blindings[i].bytes, sizeof(bits256), sizeof(bits256), M,
				    numplayers, sharenrs, space, sizeof(space));
		// create combined allshares
		for (j = 0; j < numplayers; j++) {
			// dlg_info("%s --> ",bits256_str(hexstr,cardshares[j]));
			bet_cipher_create(b_key.priv, keys[j], temp.bytes, cardshares[j].bytes, sizeof(cardshares[j]));
			memcpy(g_shares[numplayers * numcards * playerid + i * numplayers + j].bytes, temp.bytes,
			       sizeof(temp));
		}
	}
	return b_key;
}
