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
#define _POSIX_C_SOURCE 200809L /* For pclose, popen, strdup */

#include <sqlite3.h>

#include "../../includes/cJSON.h"
#include "../../includes/ppapi/c/pp_stdint.h"
#include "macrologger.h"
#include "bet.h"
#include "cards.h"
#include "client.h"
#include "commands.h"
#include "gfshare.h"
#include "network.h"
#include "payment.h"
#include "states.h"
#include "table.h"
#include "storage.h"
#include "cashier.h"
#include "misc.h"
#include "host.h"
#include "config.h"
#include "err.h"
#include "switchs.h"
#include "player.h"
// Legacy Lightning Network functions removed - use CHIPS transactions instead

#define LWS_PLUGIN_STATIC

struct lws *wsi_global_client = NULL, *wsi_global_client_write = NULL;

int ws_connection_status = 0, ws_connection_status_write = 0;

struct lws *wsi_global_bvv = NULL;

double max_allowed_dcv_commission = 2;

int32_t player_card_matrix[hand_size];
int32_t player_card_values[hand_size];
int32_t number_cards_drawn = 0;

int32_t sharesflag[CARDS_MAXCARDS][CARDS_MAXPLAYERS];

int32_t data_exists = 0;
char player_gui_data[8196];

char player_payin_txid[100];
struct deck_player_info player_info;
struct deck_bvv_info bvv_info;
int32_t no_of_shares = 0;
int32_t player_cards[CARDS_MAXCARDS];
int32_t no_of_player_cards = 0;

int32_t player_id = 0;
int32_t player_joined = 0;

bits256 all_v_hash[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXCARDS];
bits256 all_g_hash[CARDS_MAXPLAYERS][CARDS_MAXPLAYERS][CARDS_MAXCARDS];
int32_t all_sharesflag[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXPLAYERS];

int32_t all_player_card_values[CARDS_MAXPLAYERS][hand_size];
int32_t all_number_cards_drawn[CARDS_MAXPLAYERS];
int32_t all_no_of_player_cards[CARDS_MAXPLAYERS];
bits256 all_playershares[CARDS_MAXPLAYERS][CARDS_MAXCARDS][CARDS_MAXPLAYERS];

struct enc_share *all_g_shares[CARDS_MAXPLAYERS];

struct privatebet_info *bet_bvv = NULL;
struct privatebet_vars *bvv_vars = NULL;

struct privatebet_info *BET_player[CARDS_MAXPLAYERS] = { NULL };
struct deck_player_info all_players_info[CARDS_MAXPLAYERS];

char lws_buf_1[65536];
int32_t lws_buf_length_1 = 0;
char lws_buf_bvv[2000];
int32_t lws_buf_length_bvv = 0;

char req_identifier[65];
int32_t backend_status = backend_not_ready;
int32_t sitout_value = 0;
int32_t reset_lock = 0;

struct lws_context_creation_info lws_player_info, lws_player_info_read, lws_player_info_write;
struct lws_context *player_context = NULL, *player_context_read = NULL, *player_context_write = NULL;

void write_to_GUI(void *ptr)
{
	cJSON *data = ptr;

	char *json_str = cJSON_Print(data);
	dlg_info("%s\n", json_str);
	if (ws_connection_status == 1) {
		memset(player_gui_data, 0, sizeof(player_gui_data));
		size_t len = strlen(json_str);
		if (len >= sizeof(player_gui_data)) {
			len = sizeof(player_gui_data) - 1;
		}
		memcpy(player_gui_data, json_str, len);
		player_gui_data[len] = '\0';  // Ensure null termination
		data_exists = 1;
		lws_callback_on_writable(wsi_global_client);
	}
	free(json_str);
}

void player_lws_write(cJSON *data)
{
	// Check if this is a "safe" message that can be sent before backend is fully ready
	// These are wallet/status queries that don't require game state
	bool is_safe_message = false;
	const char *method = jstr(data, "method");
	if (method != NULL) {
		if (strcmp(method, "table_info") == 0 ||
		    strcmp(method, "backend_status") == 0 ||
		    strcmp(method, "bal_info") == 0) {
			is_safe_message = true;
		}
	}
	
	if (backend_status == backend_ready || is_safe_message) {
		if (ws_connection_status == 1) {
			// Wait for previous write to complete (same pattern as dealer)
		if (data_exists == 1) {
			dlg_info("Waiting for previous write to complete...");
			while (data_exists == 1) {
				sleep(1);
			}
		}
		memset(player_gui_data, 0, sizeof(player_gui_data));
		char *json_str = cJSON_Print(data);
		dlg_info("\033[34m[► TO GUI]\033[0m %s", json_str);
		size_t len = strlen(json_str);
		if (len >= sizeof(player_gui_data)) {
			len = sizeof(player_gui_data) - 1;
		}
		memcpy(player_gui_data, json_str, len);
		player_gui_data[len] = '\0';  // Ensure null termination
		free(json_str);
		data_exists = 1;
		lws_callback_on_writable(wsi_global_client);
		} else {
			dlg_warn("Backend is ready, but GUI is not started yet...");
		}
	} else {
		dlg_warn("Backend is not ready to write data to the GUI");
	}
}

char *enc_share_str(char hexstr[177], struct enc_share x)
{
	init_hexbytes_noT(hexstr, x.bytes, sizeof(x));
	return (hexstr);
}

struct enc_share get_API_enc_share(cJSON *obj)
{
	struct enc_share hash;
	char *str = NULL;
	memset(hash.bytes, 0, sizeof(hash));
	if (obj != 0) {
		if (is_cJSON_String(obj) != 0 && (str = obj->valuestring) != 0 && strlen(str) == 176) {
			decode_hex(hash.bytes, sizeof(hash), str);
		}
	}

	return (hash);
}

int32_t bet_bvv_init(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	char str[65], enc_str[177];
	cJSON *cjson_dcv_blind_cards = NULL, *cjson_peer_pubkeys = NULL, *bvv_init_info = NULL,
	      *cjson_bvv_blind_cards = NULL, *cjson_shamir_shards = NULL;
	bits256 dcv_blind_cards[CARDS_MAXPLAYERS][CARDS_MAXCARDS], peer_pubkeys[CARDS_MAXPLAYERS];
	bits256 bvv_blinding_values[CARDS_MAXPLAYERS][CARDS_MAXCARDS];
	bits256 bvv_blind_cards[CARDS_MAXPLAYERS][CARDS_MAXCARDS];

	bvv_info.deckid = jbits256(argjson, "deckid");
	bvv_info.bvv_key.priv = curve25519_keypair(&bvv_info.bvv_key.prod);
	cjson_peer_pubkeys = cJSON_GetObjectItem(argjson, "peerpubkeys");
	cjson_dcv_blind_cards = cJSON_GetObjectItem(argjson, "dcvblindcards");

	for (uint32_t i = 0; i < bvv_info.maxplayers; i++) {
		peer_pubkeys[i] = jbits256i(cjson_peer_pubkeys, i);
		for (int j = 0; j < bet->range; j++) {
			dcv_blind_cards[i][j] = jbits256i(cjson_dcv_blind_cards, i * bet->range + j);
		}
	}
	g_shares = (struct enc_share *)malloc(CARDS_MAXPLAYERS * CARDS_MAXPLAYERS * CARDS_MAXCARDS *
					      sizeof(struct enc_share));
	if (g_shares == NULL) {
		dlg_error("Memory allocation failed for g_shares");
		return ERR_MEMORY_ALLOC;
	}

	for (uint32_t i = 0; i < bvv_info.maxplayers; i++) {
		p2p_bvv_init(peer_pubkeys, bvv_info.bvv_key, bvv_blinding_values[i], bvv_blind_cards[i],
			     dcv_blind_cards[i], bet->range, bvv_info.numplayers, i, bvv_info.deckid);
	}

	bvv_init_info = cJSON_CreateObject();
	cJSON_AddStringToObject(bvv_init_info, "method", "init_b");
	jaddbits256(bvv_init_info, "bvvpubkey", bvv_info.bvv_key.prod);
	cJSON_AddItemToObject(bvv_init_info, "bvvblindcards", cjson_bvv_blind_cards = cJSON_CreateArray());
	for (uint32_t i = 0; i < bvv_info.numplayers; i++) {
		for (int j = 0; j < bet->range; j++) {
			cJSON_AddItemToArray(cjson_bvv_blind_cards,
					     cJSON_CreateString(bits256_str(str, bvv_blind_cards[i][j])));
		}
	}
	cJSON_AddItemToObject(bvv_init_info, "shamirshards", cjson_shamir_shards = cJSON_CreateArray());
	int k = 0;
	for (uint32_t playerid = 0; playerid < bvv_info.numplayers; playerid++) {
		for (int i = 0; i < bet->range; i++) {
			for (uint32_t j = 0; j < bvv_info.numplayers; j++) {
				cJSON_AddItemToArray(cjson_shamir_shards,
						     cJSON_CreateString(enc_share_str(enc_str, g_shares[k++])));
			}
		}
	}
	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

static int32_t bet_bvv_join_init(struct privatebet_info *bet)
{
	int32_t retval = OK;
	cJSON *bvv_response_info = NULL;

	bvv_response_info = cJSON_CreateObject();
	cJSON_AddStringToObject(bvv_response_info, "method", "bvv_join");
	dlg_info("BVV Response Info::%s\n", cJSON_Print(bvv_response_info));
	// Nanomsg removed - no longer used
	retval = OK;

	return retval;
}

int32_t bet_check_bvv_ready(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *bvv_ready = NULL;

	bvv_ready = cJSON_CreateObject();
	cJSON_AddStringToObject(bvv_ready, "method", "bvv_ready");

	dlg_info("BVV ready info::%s\n", cJSON_Print(bvv_ready));
	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

void bet_bvv_reset(struct privatebet_info *bet, struct privatebet_vars *vars)
{
	bet_permutation(bvv_info.permis, bet->range);
	for (int i = 0; i < bet->range; i++) {
		permis_b[i] = bvv_info.permis[i];
	}
	if (g_shares)
		free(g_shares);
}

// bet_bvv_backend_loop removed - nanomsg/pub-sub communication no longer used
// BVV functionality is now handled through Verus ID updates and websocket callbacks

int32_t bet_bvv_backend(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	char *method;
	int32_t retval = OK;

	if ((method = jstr(argjson, "method")) != 0) {
		dlg_info("received message::%s\n", method);
		if (strcmp(method, "init_d") == 0) {
			retval = bet_bvv_init(argjson, bet, vars);
		} else if (strcmp(method, "bvv_join") == 0) {
			retval = bet_bvv_join_init(bet);
		} else if (strcmp(method, "check_bvv_ready") == 0) {
			retval = bet_check_bvv_ready(argjson, bet, vars);
		} else if (strcmp(method, "reset") == 0) {
			bet_bvv_reset(bet, vars);
			retval = bet_bvv_join_init(bet);
		} else if (strcmp(method, "config_data") == 0) {
			max_players = jint(argjson, "max_players");
			chips_tx_fee = jdouble(argjson, "chips_tx_fee");
			bet->maxplayers = max_players;
			bet->numplayers = max_players;
			bvv_info.maxplayers = bet->maxplayers;
			bvv_info.numplayers = bet->numplayers;
		}
	}
	return retval;
}

bits256 bet_decode_card(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, int32_t cardid,
			int32_t *retval)
{
	int32_t numplayers, M, flag = 0;
	bits256 recover, decoded, refval, tmp, xoverz, hash, fe, basepoint;
	uint8_t **shares;
	char str[65];

	numplayers = bet->maxplayers;
	shares = calloc(numplayers, sizeof(uint8_t *));
	if (shares == NULL) {
		dlg_error("Memory allocation failed for shares");
		if (retval) *retval = ERR_MEMORY_ALLOC;
		bits256 zero = {0};
		return zero;
	}
	for (int i = 0; i < numplayers; i++) {
		shares[i] = calloc(sizeof(bits256), sizeof(uint8_t));
		if (shares[i] == NULL) {
			// Cleanup previous allocations
			for (int j = 0; j < i; j++) {
				free(shares[j]);
			}
			free(shares);
			dlg_error("Memory allocation failed for shares[%d]", i);
			if (retval) *retval = ERR_MEMORY_ALLOC;
			bits256 zero = {0};
			return zero;
		}
	}

	M = (numplayers / 2) + 1;
	for (int i = 0; i < M; i++) {
		memcpy(shares[i], playershares[cardid][i].bytes, sizeof(bits256));
	}
	gfshare_calc_sharenrs(sharenrs, numplayers, player_info.deckid.bytes,
			      sizeof(player_info.deckid)); // same for all players for this round

	gfshare_recoverdata(shares, sharenrs, M, recover.bytes, sizeof(bits256), M);

	gfshare_recoverdata(shares, sharenrs, M, recover.bytes, sizeof(bits256), M);
	refval = fmul_donna(player_info.bvvblindcards[bet->myplayerid][cardid], crecip_donna(recover));

	// dlg_info("DCV blinded card:%s",bits256_str(str,refval));

	for (int i = 0; i < bet->range; i++) {
		for (int j = 0; j < bet->range; j++) {
			bits256 temp = xoverz_donna(curve25519(player_info.player_key.priv,
							       curve25519(player_info.cardprivkeys[i],
									  player_info.cardprods[bet->myplayerid][j])));
			vcalc_sha256(0, v_hash[i][j].bytes, temp.bytes, sizeof(temp));
		}
	}

	basepoint = curve25519_basepoint9();
	for (int i = 0; i < bet->range; i++) {
		for (int j = 0; j < bet->range; j++) {
			if (bits256_cmp(v_hash[i][j], g_hash[bet->myplayerid][cardid]) == 0) {
				for (int m = 0; m < bet->range; m++) {
					for (int n = 0; n < bet->range; n++) {
						tmp = curve25519(player_info.player_key.priv,
								 curve25519(player_info.cardprivkeys[m],
									    player_info.cardprods[bet->myplayerid][n]));
						xoverz = xoverz_donna(tmp);
						vcalc_sha256(0, hash.bytes, xoverz.bytes, sizeof(xoverz));

						fe = crecip_donna(curve25519_fieldelement(hash));

						decoded = curve25519(fmul_donna(refval, fe), basepoint);
						for (int k = 0; k < bet->range; k++) {
							if (bits256_cmp(decoded,
									player_info.cardprods[bet->myplayerid][k]) ==
							    0) {
								player_cards[no_of_player_cards] =
									atoi(bits256_str(str, decoded));
								no_of_player_cards++;
								tmp = player_info.cardprivkeys[m];
								flag = 1;
								goto end;
							}
						}
					}
				}
			}
		}
	}

end:
	if (!flag)
		*retval = ERR_CARD_RETRIEVING_USING_SS;

	return tmp;
}

// Legacy Lightning Network functions - stubs (to be replaced with Verus/CHIPS)
static int32_t bet_player_betting_invoice(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	dlg_warn("bet_player_betting_invoice: Lightning Network deprecated - use CHIPS transactions instead");
	(void)argjson; (void)bet; (void)vars;
	return ERR_LN;
}

static int32_t bet_player_create_invoice(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, char *deckid)
{
	dlg_warn("bet_player_create_invoice: Lightning Network deprecated - use CHIPS transactions instead");
	(void)argjson; (void)bet; (void)vars; (void)deckid;
	return ERR_LN;
}

static int32_t bet_player_winner(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int argc, retval = OK;
	char **argv = NULL, hexstr[65], params[arg_size];
	cJSON *invoice_info = NULL, *winner_invoice_info = NULL;

	if (jint(argjson, "playerid") == bet->myplayerid) {
		argc = 5;
		bet_alloc_args(argc, &argv);

		const char *deckid_str = bits256_str(hexstr, player_info.deckid);
		int winning_amount = jint(argjson, "winning_amount");
		snprintf(params, sizeof(params), "%s_%d", deckid_str, winning_amount);
		argv = bet_copy_args(argc, "lightning-cli", "invoice", jint(argjson, "winning_amount"), params,
				     "Winning claim");

		winner_invoice_info = cJSON_CreateObject();
		retval = make_command(argc, argv, &winner_invoice_info);
		if (retval != OK) {
			dlg_error("%s", bet_err_str(retval));
		}
		invoice_info = cJSON_CreateObject();
		cJSON_AddStringToObject(invoice_info, "method", "claim");
		cJSON_AddNumberToObject(invoice_info, "playerid", bet->myplayerid);
		cJSON_AddStringToObject(invoice_info, "label", params);
		cJSON_AddStringToObject(invoice_info, "invoice", cJSON_Print(winner_invoice_info));

		// Nanomsg removed - no longer used
		retval = OK;
	}
	bet_dealloc_args(argc, &argv);
	return retval;
}

void display_cards()
{
	char *cards[52] = { "2C", "3C", "4C", "5C", "6C", "7C", "8C", "9C", "10C", "JC", "QC", "KC", "AC",
			    "2D", "3D", "4D", "5D", "6D", "7D", "8D", "9D", "10D", "JD", "QD", "KD", "AD",
			    "2H", "3H", "4H", "5H", "6H", "7H", "8H", "9H", "10H", "JH", "QH", "KH", "AH",
			    "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "10S", "JS", "QS", "KS", "AS" };

	cJSON *init_card_info = NULL, *hole_card_info = NULL, *init_info = NULL, *board_card_info = NULL;

	init_info = cJSON_CreateObject();
	cJSON_AddStringToObject(init_info, "method", "deal");

	init_card_info = cJSON_CreateObject();
	cJSON_AddNumberToObject(init_card_info, "dealer", 0);

	hole_card_info = cJSON_CreateArray();
	for (int32_t i = 0; ((i < no_of_hole_cards) && (i < number_cards_drawn)); i++) {
		cJSON_AddItemToArray(hole_card_info, cJSON_CreateString(cards[player_card_values[i]]));
	}

	cJSON_AddItemToObject(init_card_info, "holecards", hole_card_info);

	board_card_info = cJSON_CreateArray();
	for (int32_t i = no_of_hole_cards; ((i < hand_size) && (i < number_cards_drawn)); i++) {
		cJSON_AddItemToArray(board_card_info, cJSON_CreateString(cards[player_card_values[i]]));
	}

	cJSON_AddItemToObject(init_card_info, "board", board_card_info);
	cJSON_AddItemToObject(init_info, "deal", init_card_info);
	player_lws_write(init_info);
}

int32_t bet_client_receive_share(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, cardid, playerid, errs = 0, unpermi, card_type;
	cJSON *player_card_info = NULL;
	bits256 share, decoded256;

	share = jbits256(argjson, "share");
	cardid = jint(argjson, "cardid");
	playerid = jint(argjson, "playerid");
	card_type = jint(argjson, "card_type");

	if (sharesflag[cardid][playerid] == 0) {
		playershares[cardid][playerid] = share;
		sharesflag[cardid][playerid] = 1;
		no_of_shares++;
	}
	if ((no_of_shares < bet->maxplayers) && (jint(argjson, "to_playerid") == bet->myplayerid)) {
		for (int i = 0; i < bet->maxplayers; i++) {
			if ((!sharesflag[jint(argjson, "cardid")][i]) && (i != bet->myplayerid)) {
				retval = bet_player_ask_share(bet, jint(argjson, "cardid"), bet->myplayerid,
							      jint(argjson, "card_type"), i);
				break;
			}
		}
	} else if (no_of_shares == bet->maxplayers) {
		no_of_shares = 0;
		decoded256 = bet_decode_card(argjson, bet, vars, cardid, &retval);
		if (retval != OK)
			return retval;

		if (bits256_nonz(decoded256) == 0)
			errs++;
		else {
			unpermi = -1;
			for (int k = 0; k < bet->range; k++) {
				if (player_info.permis[k] == decoded256.bytes[30]) {
					unpermi = k;
					break;
				}
			}
		}
		if (unpermi != -1) {
			player_card_values[number_cards_drawn++] = decoded256.bytes[30];
			player_card_info = cJSON_CreateObject();
			cJSON_AddStringToObject(player_card_info, "method", "playerCardInfo");
			cJSON_AddNumberToObject(player_card_info, "playerid", bet->myplayerid);
			cJSON_AddNumberToObject(player_card_info, "cardid", cardid);
			cJSON_AddNumberToObject(player_card_info, "card_type", card_type);
			cJSON_AddNumberToObject(player_card_info, "decoded_card", decoded256.bytes[30]);
			dlg_info("%s", cJSON_Print(player_card_info));
			// Nanomsg removed - no longer used
			retval = OK;
		}
	}
	return retval;
}

int32_t bet_player_ask_share(struct privatebet_info *bet, int32_t cardid, int32_t playerid, int32_t card_type,
			     int32_t other_player)
{
	cJSON *request_info = NULL;
	int32_t retval = OK;

	request_info = cJSON_CreateObject();
	cJSON_AddStringToObject(request_info, "method", "requestShare");
	cJSON_AddNumberToObject(request_info, "playerid", playerid);
	cJSON_AddNumberToObject(request_info, "cardid", cardid);
	cJSON_AddNumberToObject(request_info, "card_type", card_type);
	cJSON_AddNumberToObject(request_info, "other_player", other_player);

	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

int32_t bet_client_give_share(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid, cardid, recvlen, card_type;
	cJSON *share_info = NULL;
	struct enc_share temp;
	uint8_t decipher[sizeof(bits256) + 1024], *ptr;
	bits256 share;

	playerid = jint(argjson, "playerid");
	cardid = jint(argjson, "cardid");
	card_type = jint(argjson, "card_type");

	if (playerid == bet->myplayerid) {
		return retval;
	}

	temp = g_shares[playerid * bet->numplayers * bet->range + (cardid * bet->numplayers + bet->myplayerid)];

	recvlen = sizeof(temp);

	if ((ptr = bet_decrypt(decipher, sizeof(decipher), player_info.bvvpubkey, player_info.player_key.priv,
			       temp.bytes, &recvlen)) == 0) {
		retval = ERR_DECRYPTING_OTHER_SHARE;
	}

	share_info = cJSON_CreateObject();
	cJSON_AddStringToObject(share_info, "method", "share_info");
	cJSON_AddNumberToObject(share_info, "playerid", bet->myplayerid);
	cJSON_AddNumberToObject(share_info, "cardid", cardid);
	cJSON_AddNumberToObject(share_info, "card_type", card_type);
	cJSON_AddNumberToObject(share_info, "to_playerid", playerid);
	cJSON_AddNumberToObject(share_info, "error", retval);

	if (retval != ERR_DECRYPTING_OTHER_SHARE) {
		memcpy(share.bytes, ptr, recvlen);
		jaddbits256(share_info, "share", share);
	}
	// Nanomsg removed - no longer used
	retval = OK;

	return retval;
}

int32_t bet_get_own_share(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	struct enc_share temp;
	int32_t cardid, retval = OK, playerid, recvlen;
	uint8_t decipher[sizeof(bits256) + 1024], *ptr;
	bits256 share;

	playerid = jint(argjson, "playerid");
	cardid = jint(argjson, "cardid");

	temp = g_shares[bet->myplayerid * bet->numplayers * bet->range + (cardid * bet->numplayers + playerid)];
	recvlen = sizeof(temp);

	if ((ptr = bet_decrypt(decipher, sizeof(decipher), player_info.bvvpubkey, player_info.player_key.priv,
			       temp.bytes, &recvlen)) == 0) {
		retval = ERR_DECRYPTING_OWN_SHARE;
	} else {
		memcpy(share.bytes, ptr, recvlen);
		playershares[cardid][bet->myplayerid] = share;
		sharesflag[cardid][bet->myplayerid] = 1;
	}
	return retval;
}

int32_t bet_client_turn(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid;

	playerid = jint(argjson, "playerid");
	dlg_info("playerid::%d::%s::\n", playerid, cJSON_Print(argjson));

	if (playerid == bet->myplayerid) {
		no_of_shares = 1;
		if ((retval = bet_get_own_share(argjson, bet, vars)) != OK)
			return retval;

		for (int i = 0; i < bet->numplayers; i++) {
			if ((!sharesflag[jint(argjson, "cardid")][i]) && (i != bet->myplayerid)) {
				retval = bet_player_ask_share(bet, jint(argjson, "cardid"), jint(argjson, "playerid"),
							      jint(argjson, "card_type"), i);
				break;
			}
		}
	}
	return retval;
}

int32_t bet_player_ready(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	cJSON *player_ready = NULL;
	int retval = OK;

	player_ready = cJSON_CreateObject();
	cJSON_AddStringToObject(player_ready, "method", "player_ready");
	cJSON_AddNumberToObject(player_ready, "playerid", bet->myplayerid);

	// Nanomsg removed - no longer used
	retval = OK;
	return retval;
}

int32_t bet_client_bvv_init(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)

{
	int32_t retval = OK;
	cJSON *cjson_bvv_blind_cards, *cjson_shamir_shards;
	bits256 temp, player_privs[CARDS_MAXCARDS];

	player_info.bvvpubkey = jbits256(argjson, "bvvpubkey");
	g_shares = (struct enc_share *)malloc(CARDS_MAXPLAYERS * CARDS_MAXPLAYERS * CARDS_MAXCARDS *
					      sizeof(struct enc_share));
	if (g_shares == NULL) {
		dlg_error("Memory allocation failed for g_shares");
		return ERR_MEMORY_ALLOC;
	}
	cjson_bvv_blind_cards = cJSON_GetObjectItem(argjson, "bvvblindcards");

	for (int i = 0; i < bet->numplayers; i++) {
		for (int j = 0; j < bet->range; j++) {
			player_info.bvvblindcards[i][j] = jbits256i(cjson_bvv_blind_cards, i * bet->range + j);
		}
	}

	cjson_shamir_shards = cJSON_GetObjectItem(argjson, "shamirshards");
	int k = 0;
	for (int playerid = 0; playerid < bet->numplayers; playerid++) {
		for (int i = 0; i < bet->range; i++) {
			for (int j = 0; j < bet->numplayers; j++) {
				g_shares[k] = get_API_enc_share(cJSON_GetArrayItem(cjson_shamir_shards, k));
				k++;
			}
		}
	}

	for (int i = 0; i < bet->range; i++) {
		for (int j = 0; j < bet->range; j++) {
			temp = xoverz_donna(
				curve25519(player_info.player_key.priv,
					   curve25519(player_privs[i], player_info.cardprods[bet->myplayerid][j])));
			vcalc_sha256(0, v_hash[i][j].bytes, temp.bytes, sizeof(temp));
		}
	}
	retval = bet_player_ready(argjson, bet, vars);
	return retval;
}

int32_t bet_client_dcv_init(cJSON *dcv_info, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *cjson_card_prods, *cjsong_hash;

	player_info.deckid = jbits256(dcv_info, "deckid");
	cjson_card_prods = cJSON_GetObjectItem(dcv_info, "cardprods");

	for (int i = 0; i < bet->numplayers; i++) {
		for (int j = 0; j < bet->range; j++) {
			player_info.cardprods[i][j] = jbits256i(cjson_card_prods, i * bet->range + j);
		}
	}

	cjsong_hash = cJSON_GetObjectItem(dcv_info, "g_hash");
	for (int i = 0; i < bet->numplayers; i++) {
		for (int j = 0; j < bet->range; j++) {
			g_hash[i][j] = jbits256i(cjsong_hash, i * bet->range + j);
		}
	}

	return retval;
}

int32_t bet_client_init(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *cjson_player_cards, *init_p = NULL;
	char str[65];

	init_p = cJSON_CreateObject();

	cJSON_AddStringToObject(init_p, "method", "init_p");
	cJSON_AddNumberToObject(init_p, "peerid", bet->myplayerid);
	jaddbits256(init_p, "pubkey", player_info.player_key.prod);
	cJSON_AddItemToObject(init_p, "cardinfo", cjson_player_cards = cJSON_CreateArray());
	for (int i = 0; i < bet->range; i++) {
		cJSON_AddItemToArray(cjson_player_cards,
				     cJSON_CreateString(bits256_str(str, player_info.cardpubkeys[i])));
	}
	// Nanomsg removed - no longer used
	retval = OK;

	return retval;
}

// Lightning Network code removed - bet_establish_ln_channel_with_dealer() no longer used

int32_t bet_client_join_res(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *init_card_info = NULL, *hole_card_info = NULL, *init_info = NULL;

	if (0 == bits256_cmp(player_info.player_key.prod, jbits256(argjson, "pubkey"))) {
		bet_player->myplayerid = jint(argjson, "playerid");
		bet->myplayerid = jint(argjson, "playerid");
		dlg_info("%s", cJSON_Print(argjson));

		// Lightning Network support removed - channel establishment no longer used

		init_card_info = cJSON_CreateObject();
		cJSON_AddNumberToObject(init_card_info, "dealer", jint(argjson, "dealer"));

		cJSON_AddNumberToObject(init_card_info, "balance", vars->player_funds);

		hole_card_info = cJSON_CreateArray();
		cJSON_AddItemToArray(hole_card_info, cJSON_CreateNull());
		cJSON_AddItemToArray(hole_card_info, cJSON_CreateNull());
		cJSON_AddItemToObject(init_card_info, "holecards", hole_card_info);

		init_info = cJSON_CreateObject();
		cJSON_AddStringToObject(init_info, "method", "deal");
		cJSON_AddItemToObject(init_info, "deal", init_card_info);

		dlg_info("init_info::%s", cJSON_Print(init_info));
		player_lws_write(init_info);
	}
	return retval;
}

int32_t bet_client_join(cJSON *argjson, struct privatebet_info *bet)
{
	int32_t retval = OK;
	char *uri = NULL;
	cJSON *joininfo = NULL;
	struct pair256 key;

	if ((jint(argjson, "gui_playerID") < 1) || (jint(argjson, "gui_playerID") > bet->maxplayers)) {
		retval = ERR_INVALID_POS;
		return retval;
	}
	key = deckgen_player(player_info.cardprivkeys, player_info.cardpubkeys, player_info.permis, bet->range);
	player_info.player_key = key;
	joininfo = cJSON_CreateObject();
	cJSON_AddStringToObject(joininfo, "method", "join_req");
	jaddbits256(joininfo, "pubkey", key.prod);

	// Lightning Network support removed - uri field no longer added

	cJSON_AddNumberToObject(joininfo, "gui_playerID", (jint(argjson, "gui_playerID") - 1));
	cJSON_AddStringToObject(joininfo, "req_identifier", req_identifier);
	cJSON_AddStringToObject(joininfo, "player_name", player_name);

	dlg_info("join info::%s\n", cJSON_Print(joininfo));
	// Nanomsg removed - no longer used
	retval = OK;

end:
	if (uri)
		free(uri);
	return retval;
}

static int32_t bet_player_process_player_join(cJSON *argjson)
{
	int32_t retval = OK;
	cJSON *warning_info = NULL;

	if (player_joined == 0) {
		if (backend_status == backend_ready) {
			retval = bet_client_join(argjson, bet_player);
		} else {
			warning_info = cJSON_CreateObject();
			cJSON_AddStringToObject(warning_info, "method", "warning");
			cJSON_AddNumberToObject(warning_info, "warning_num", backend_not_ready);
			dlg_warn("%s\n", cJSON_Print(warning_info));
			player_lws_write(warning_info);
		}
	}
	return retval;
}

static void bet_player_handle_invalid_method(char *method)
{
	cJSON *error_info = NULL;
	char error_msg[2048];

	error_info = cJSON_CreateObject();
	cJSON_AddStringToObject(error_info, "method", "error");
	snprintf(error_msg, sizeof(error_msg), "Method::%s is not handled", method);
	cJSON_AddStringToObject(error_info, "msg", error_msg);
	player_lws_write(error_info);
}

// bet_player_withdraw_request and bet_player_withdraw removed - now using common wallet handler bet_wallet_withdraw_request() and bet_wallet_withdraw()

void bet_player_table_info(void)
{
	cJSON *table_info = NULL;
	cJSON *occupied_seats = NULL;
	cJSON *t_player_info = NULL;
	cJSON *player_info_array = NULL;
	
	table_info = cJSON_CreateObject();
	cJSON_AddStringToObject(table_info, "method", "table_info");
	cJSON_AddStringToObject(table_info, "addr", chips_get_wallet_address());
	cJSON_AddNumberToObject(table_info, "balance", chips_get_balance());
	cJSON_AddNumberToObject(table_info, "backend_status", backend_status);
	cJSON_AddNumberToObject(table_info, "max_players", max_players);
	cJSON_AddNumberToObject(table_info, "table_min_stake", table_min_stake);  // Payin amount in CHIPS
	cJSON_AddNumberToObject(table_info, "small_blind", SB_in_chips);
	cJSON_AddNumberToObject(table_info, "big_blind", BB_in_chips);
	cJSON_AddStringToObject(table_info, "table_id", player_config.table_id);
	cJSON_AddStringToObject(table_info, "dealer_id", player_config.dealer_id);
	
	// Get occupied seats from blockchain
	occupied_seats = cJSON_CreateArray();
	if (strlen(player_config.table_id) > 0) {
		extern cJSON *get_t_player_info(char *table_id);
		t_player_info = get_t_player_info(player_config.table_id);
		if (t_player_info) {
			player_info_array = cJSON_GetObjectItem(t_player_info, "player_info");
			cJSON *payin_amounts = cJSON_GetObjectItem(t_player_info, "payin_amounts");
			if (player_info_array && player_info_array->type == cJSON_Array) {
				int array_size = cJSON_GetArraySize(player_info_array);
				for (int i = 0; i < array_size; i++) {
					char *player_entry = cJSON_GetArrayItem(player_info_array, i)->valuestring;
					// Format: "verus_pid_txid_slot"
					// Extract slot number (last part after last underscore)
					char *last_underscore = strrchr(player_entry, '_');
					if (last_underscore) {
						int seat_num = atoi(last_underscore + 1);
						// Extract player ID (first part before first underscore)
						char player_id[128] = {0};
						char *first_underscore = strchr(player_entry, '_');
						if (first_underscore) {
							int pid_len = first_underscore - player_entry;
							if (pid_len < 128) {
								strncpy(player_id, player_entry, pid_len);
								player_id[pid_len] = '\0';
								
								cJSON *seat_obj = cJSON_CreateObject();
								cJSON_AddNumberToObject(seat_obj, "seat", seat_num);
								cJSON_AddStringToObject(seat_obj, "player_id", player_id);
								
								// Add payin amount (stack) from parallel array
								if (payin_amounts && i < cJSON_GetArraySize(payin_amounts)) {
									cJSON *amount = cJSON_GetArrayItem(payin_amounts, i);
									if (amount) {
										cJSON_AddNumberToObject(seat_obj, "stack", amount->valuedouble);
									}
								}
								
								cJSON_AddItemToArray(occupied_seats, seat_obj);
							}
						}
					}
				}
			}
			cJSON_Delete(t_player_info);
		}
	}
	cJSON_AddItemToObject(table_info, "occupied_seats", occupied_seats);
	
	dlg_info("%s\n", cJSON_Print(table_info));
	player_lws_write(table_info);
	cJSON_Delete(table_info);
}

static void bet_player_process_be_status()
{
	cJSON *be_info = NULL;

	be_info = cJSON_CreateObject();
	cJSON_AddStringToObject(be_info, "method", "backend_status");
	cJSON_AddNumberToObject(be_info, "backend_status", backend_status);
	player_lws_write(be_info);
}

void send_init_state_to_gui(int32_t state)
{
	extern int32_t player_init_state;
	extern const char *player_init_state_str(int32_t state);
	
	if (g_betting_mode != BET_MODE_GUI) {
		return;  // Only send to GUI in GUI mode
	}
	
	player_init_state = state;
	
	cJSON *state_msg = cJSON_CreateObject();
	cJSON_AddStringToObject(state_msg, "method", "player_init_state");
	cJSON_AddNumberToObject(state_msg, "state", state);
	cJSON_AddStringToObject(state_msg, "state_name", player_init_state_str(state));
	
	// Add player_id and payin info for JOINED state
	if (state == P_INIT_JOINED) {
		cJSON_AddStringToObject(state_msg, "player_id", player_config.verus_pid);
		// Player knows their own payin amount - use config value directly
		// (blockchain t_player_info may not be updated yet at this point)
		cJSON_AddNumberToObject(state_msg, "payin_amount", table_min_stake);
	}
	
	dlg_info("\033[34m[► TO GUI]\033[0m Player state: %s", player_init_state_str(state));
	player_lws_write(state_msg);
	cJSON_Delete(state_msg);
}

// clang-format off
int32_t bet_player_frontend(struct lws *wsi, cJSON *argjson)
{
	int32_t retval = OK;
	char *method = NULL;

	method = jstr(argjson,"method");
	switchs(method) {
		dlg_info("\033[32m[◄ FROM GUI]\033[0m %s", cJSON_Print(argjson));
		cases("backend_status")
			bet_player_process_be_status();
			break;
		cases("betting")
			retval = bet_player_round_betting(argjson, bet_player, player_vars);
			break;
		cases("get_bal_info")
			{
				// Common wallet functionality
				cJSON *bal_info = bet_wallet_get_bal_info();
				if (bal_info != NULL) {
					player_lws_write(bal_info);
					cJSON_Delete(bal_info);
				}
			}
			break;
		cases("player_join")
			retval = bet_player_process_player_join(argjson);
			break;
		cases("reset")
			retval = bet_player_reset(bet_player, player_vars);
			break;
		cases("sitout")
			sitout_value = jint(argjson, "value");
			break;
		cases("table_info")
			{
				dlg_info("Received table_info request from GUI");
				
				// Signal backend thread to proceed with finding table
				pthread_mutex_lock(&gui_table_mutex);
				gui_table_requested = 1;
				pthread_cond_signal(&gui_table_cond);
				pthread_mutex_unlock(&gui_table_mutex);
				dlg_info("Signaled backend to find table");
				
				// Send current table info to GUI
				bet_player_table_info();
			}
			break;
		cases("join_table")
			{
				dlg_info("Received join_table command from GUI");
				
				// Signal backend thread to proceed with join
				dlg_info("Acquiring gui_join_mutex to signal backend thread...");
				pthread_mutex_lock(&gui_join_mutex);
				dlg_info("Setting gui_join_approved = 1 and signaling condition variable...");
				gui_join_approved = 1;
				pthread_cond_signal(&gui_join_cond);
				pthread_mutex_unlock(&gui_join_mutex);
				dlg_info("Signal sent to backend thread");
				
				// Send acknowledgment to GUI
				cJSON *join_ack = cJSON_CreateObject();
				cJSON_AddStringToObject(join_ack, "method", "join_ack");
				cJSON_AddStringToObject(join_ack, "status", "approved");
				cJSON_AddStringToObject(join_ack, "message", "Join approved, proceeding with payin transaction");
				player_lws_write(join_ack);
				cJSON_Delete(join_ack);
			}
			break;
		cases("withdraw")
			{
				// Common wallet functionality
				cJSON *withdraw_info = bet_wallet_withdraw(argjson);
				if (withdraw_info != NULL) {
					player_lws_write(withdraw_info);
					cJSON_Delete(withdraw_info);
				}
			}
			break;
		cases("withdrawRequest")
			{
				// Common wallet functionality
				cJSON *withdraw_response = bet_wallet_withdraw_request();
				if (withdraw_response != NULL) {
					player_lws_write(withdraw_response);
					cJSON_Delete(withdraw_response);
				}
			}
			break;
		defaults
			bet_player_handle_invalid_method(method);
	}switchs_end;
	
	if (retval != OK)
		bet_handle_player_error(bet_player, retval);
	return retval;
}
// clang-format on

static void bet_gui_init_message(struct privatebet_info *bet)
{
	cJSON *init_info = NULL;

	// Send actual backend_status (1 if wallet+ID verified, 0 if not)
	init_info = cJSON_CreateObject();
	cJSON_AddStringToObject(init_info, "method", "backend_status");
	cJSON_AddNumberToObject(init_info, "backend_status", backend_status);
	
	if (backend_status == backend_ready) {
		cJSON_AddStringToObject(init_info, "message", "Backend ready!");
		dlg_info("GUI connected. Backend status: READY");
	} else {
		cJSON_AddStringToObject(init_info, "message", "Backend initializing...");
		dlg_info("GUI connected. Backend status: NOT READY");
	}
	
	player_lws_write(init_info);
}

int32_t lws_callback_http_player_write(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
				       size_t len)
{
	int32_t retval = OK;
	cJSON *argjson = NULL;

	switch (reason) {
	case LWS_CALLBACK_RECEIVE:
		memcpy(lws_buf_1 + lws_buf_length_1, in, len);
		lws_buf_length_1 += len;
		if (!lws_is_final_fragment(wsi))
			break;
		argjson = cJSON_Parse(lws_buf_1);

		if ((retval = bet_player_frontend(wsi, argjson)) != OK) {
			dlg_error("%s", bet_err_str(retval));
		}
		memset(lws_buf_1, 0x00, sizeof(lws_buf_1));
		lws_buf_length_1 = 0;

		break;
	case LWS_CALLBACK_ESTABLISHED:
		wsi_global_client_write = wsi;
		ws_connection_status_write = 1;
		bet_gui_init_message(bet_player);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (data_exists) {
			if (strlen(player_gui_data) != 0) {
				lws_write(wsi, (unsigned char *)player_gui_data, strlen(player_gui_data), 0);
				data_exists = 0;
			}
		}
		break;
	default:
		break;
	}
	return retval;
}

int32_t lws_callback_http_player_read(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
				      size_t len)
{
	int32_t retval = OK;
	cJSON *argjson = NULL;

	switch (reason) {
	case LWS_CALLBACK_RECEIVE:
		memcpy(lws_buf_1 + lws_buf_length_1, in, len);
		lws_buf_length_1 += len;
		if (!lws_is_final_fragment(wsi))
			break;

		argjson = cJSON_Parse(unstringify(lws_buf_1));
		if ((retval = bet_player_frontend(wsi, argjson)) != OK) {
			dlg_error("%s", bet_err_str(retval));
		}
		memset(lws_buf_1, 0x00, sizeof(lws_buf_1));
		lws_buf_length_1 = 0;

		break;
	case LWS_CALLBACK_ESTABLISHED:
		wsi_global_client = wsi;
		ws_connection_status = 1;
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (data_exists) {
			if (strlen(player_gui_data) != 0) {
				lws_write(wsi, (unsigned char *)player_gui_data, strlen(player_gui_data), 0);
				data_exists = 0;
			}
		}
		break;
	default:
		break;
	}
	return retval;
}

int32_t lws_callback_http_player(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int32_t retval = OK;
	cJSON *argjson = NULL;

	//dlg_info("callback code::%d\n", (int)reason);
	switch (reason) {
	case LWS_CALLBACK_RECEIVE:
		memcpy(lws_buf_1 + lws_buf_length_1, in, len);
		lws_buf_length_1 += len;
		if (!lws_is_final_fragment(wsi))
			break;
		argjson = cJSON_Parse(lws_buf_1);

		if ((retval = bet_player_frontend(wsi, argjson)) != OK) {
			dlg_error("%s", bet_err_str(retval));
		}
		memset(lws_buf_1, 0x00, sizeof(lws_buf_1));
		lws_buf_length_1 = 0;

		break;
	case LWS_CALLBACK_ESTABLISHED:
		wsi_global_client = wsi;
		dlg_info("LWS_CALLBACK_ESTABLISHED\n");
		ws_connection_status = 1;
		bet_gui_init_message(bet_player);
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		dlg_info("LWS_CALLBACK_SERVER_WRITEABLE triggered\n");
		if (data_exists) {
			if (strlen(player_gui_data) != 0) {
				lws_write(wsi, (unsigned char *)player_gui_data, strlen(player_gui_data), 0);
				data_exists = 0;
			}
		}
		break;
	default:
		break;
	}
	return retval;
}

static struct lws_protocols player_http_protocol[] = {
	{ "http", lws_callback_http_player, 0, 0 },
	{ NULL, NULL, 0, 0 } /* terminator */
};

static struct lws_protocols player_http_protocol_read[] = {
	{ "http", lws_callback_http_player_read, 0, 0 },
	{ NULL, NULL, 0, 0 } /* terminator */
};

static struct lws_protocols player_http_protocol_write[] = {
	{ "http", lws_callback_http_player_write, 0, 0 },
	{ NULL, NULL, 0, 0 } /* terminator */
};

static int interrupted1, interrupted_read, interrupted_write;

static const struct lws_http_mount lws_http_mount_player = {
	/* .mount_next */ NULL, /* linked-list "next" */
	/* .mountpoint */ "/", /* mountpoint URL */
	/* .origin */ "./mount-origin", /* serve from dir */
	/* .def */ "index.html", /* default filename */
	/* .protocol */ NULL,
	/* .cgienv */ NULL,
	/* .extra_mimetypes */ NULL,
	/* .interpret */ NULL,
	/* .cgi_timeout */ 0,
	/* .cache_max_age */ 0,
	/* .auth_mask */ 0,
	/* .cache_reusable */ 0,
	/* .cache_revalidate */ 0,
	/* .cache_intermediaries */ 0,
	/* .origin_protocol */ LWSMPRO_FILE, /* files in a dir */
	/* .mountpoint_len */ 1, /* char count */
	/* .basic_auth_login_file */ NULL,
};

void player_sigint_handler(int sig)
{
	interrupted1 = 1;
}

void player_sigint_handler_read(int sig)
{
	interrupted_read = 1;
}

void player_sigint_handler_write(int sig)
{
	interrupted_write = 1;
}

void bet_player_frontend_read_loop(void *_ptr)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	lws_set_log_level(logs, NULL);
	dlg_info("[GUI] Player WebSocket read server starting on port 9001");

	memset(&lws_player_info_read, 0, sizeof lws_player_info_read); /* otherwise uninitialized garbage */
	lws_player_info_read.port = 9001;
	lws_player_info_read.mounts = &lws_http_mount_player;
	lws_player_info_read.protocols = player_http_protocol_read;
	lws_player_info_read.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	lws_player_info_read.uid = -1;  /* Don't drop privileges */
	lws_player_info_read.gid = -1;  /* Don't drop privileges */

	player_context_read = lws_create_context(&lws_player_info_read);
	if (!player_context_read) {
		dlg_error("[GUI] Player read lws init failed");
		return;
	}
	
	dlg_info("[GUI] Player WebSocket read server started successfully");
	
	// Keep running even if main thread exits
	while (n >= 0) {
		n = lws_service(player_context_read, 1000);
	}
	
	dlg_info("[GUI] Player WebSocket read server shutting down");
	if (player_context_read) {
		lws_context_destroy(player_context_read);
	}
}

void bet_player_frontend_write_loop(void *_ptr)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	// signal(SIGINT,player_sigint_handler);
	lws_set_log_level(logs, NULL);
	dlg_info("[GUI] Player WebSocket write server starting on port %d", gui_ws_port);

	memset(&lws_player_info_write, 0, sizeof lws_player_info_write); /* otherwise uninitialized garbage */
	lws_player_info_write.port = gui_ws_port;
	lws_player_info_write.mounts = &lws_http_mount_player;
	lws_player_info_write.protocols = player_http_protocol_write;
	lws_player_info_write.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	lws_player_info_write.uid = -1;  /* Don't drop privileges */
	lws_player_info_write.gid = -1;  /* Don't drop privileges */

	if (lws_player_info_write.port <= 0 || lws_player_info_write.port > 65535) {
		dlg_error("[GUI] Invalid port %d, using default %d", lws_player_info_write.port, DEFAULT_GUI_WS_PORT);
		lws_player_info_write.port = DEFAULT_GUI_WS_PORT;
	}

	player_context_write = lws_create_context(&lws_player_info_write);
	if (!player_context_write) {
		dlg_error("[GUI] Player write lws init failed");
		return;
	}
	
	dlg_info("[GUI] Player WebSocket write server started successfully");
	
	// Keep running even if main thread exits
	while (n >= 0) {
		n = lws_service(player_context_write, 1000);
	}
	
	dlg_info("[GUI] Player WebSocket write server shutting down");
	if (player_context_write) {
		lws_context_destroy(player_context_write);
	}
}

void bet_player_frontend_loop(void *_ptr)
{
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	lws_set_log_level(logs, NULL);

	memset(&lws_player_info, 0, sizeof lws_player_info); /* otherwise uninitialized garbage */
	lws_player_info.port = gui_ws_port;
	lws_player_info.mounts = &lws_http_mount_player;
	lws_player_info.protocols = player_http_protocol;
	lws_player_info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
	lws_player_info.uid = -1;  /* Don't drop privileges */
	lws_player_info.gid = -1;  /* Don't drop privileges */
	// vhost_name is optional - leave it NULL to use default

	if (lws_player_info.port <= 0 || lws_player_info.port > 65535) {
		dlg_error("[GUI] Invalid port %d, using default %d", lws_player_info.port, DEFAULT_PLAYER_WS_PORT);
		lws_player_info.port = DEFAULT_PLAYER_WS_PORT;
	}

	player_context = lws_create_context(&lws_player_info);
	if (!player_context) {
		dlg_error("[GUI] Player lws init failed");
		return;
	}
	
	dlg_info("[GUI] Player WebSocket server started successfully on port %d", gui_ws_port);
	
	// Keep running even if main thread exits
	while (n >= 0) {
		n = lws_service(player_context, 1000);
	}
	
	dlg_info("[GUI] Player WebSocket server shutting down");
	if (player_context) {
		lws_context_destroy(player_context);
		player_context = NULL;
	}
}

void bet_push_client(cJSON *argjson)
{
	player_lws_write(argjson);
}

static void bet_player_blinds_info()
{
	cJSON *blinds_info = NULL;

	blinds_info = cJSON_CreateObject();
	cJSON_AddStringToObject(blinds_info, "method", "blindsInfo");
	cJSON_AddNumberToObject(blinds_info, "small_blind", small_blind_amount);
	cJSON_AddNumberToObject(blinds_info, "big_blind", big_blind_amount);
	player_lws_write(blinds_info);
}

static void bet_push_join_info(cJSON *argjson)
{
	cJSON *join_info = NULL;

	join_info = cJSON_CreateObject();
	cJSON_AddStringToObject(join_info, "method", "info"); //changed to join_info to info
	cJSON_AddNumberToObject(join_info, "playerid", jint(argjson, "playerid"));
	if (jint(argjson, "pos_status") == pos_on_table_full)
		cJSON_AddNumberToObject(join_info, "seat_taken", 1);
	else
		cJSON_AddNumberToObject(join_info, "seat_taken", 0);
	dlg_info("Writing the availability of the seat info to the GUI \n %s", cJSON_Print(join_info));
	player_lws_write(join_info);
}

static int32_t bet_update_payin_tx_across_cashiers(cJSON *argjson, cJSON *txid)
{
	int32_t retval = OK;
	char *sql_query = NULL;
	cJSON *msig_addr_nodes = NULL, *payin_tx_insert_query = NULL;

	const char *txid_str = cJSON_Print(txid);
	const char *msig_addr_nodes_str = cJSON_Print(cJSON_GetObjectItem(argjson, "msig_addr_nodes"));
	int required_size;

	if (txid_str == NULL || msig_addr_nodes_str == NULL) {
		dlg_error("Invalid txid or msig_addr_nodes");
		if (txid_str) free((void *)txid_str);
		if (msig_addr_nodes_str) free((void *)msig_addr_nodes_str);
		return ERR_ARGS_NULL;
	}

	required_size = snprintf(NULL, 0, "INSERT INTO player_tx_mapping values(%s,\'%s\',\'%s\',\'%s\',%d,%d, NULL);",
		txid_str, table_id, req_identifier, msig_addr_nodes_str, tx_unspent, threshold_value) + 1;

	if (required_size > sql_query_size) {
		sql_query = calloc(required_size, sizeof(char));
	} else {
		sql_query = calloc(sql_query_size, sizeof(char));
	}
	if (sql_query == NULL) {
		free((void *)txid_str);
		free((void *)msig_addr_nodes_str);
		return ERR_MEMORY_ALLOC;
	}

	snprintf(sql_query, required_size, "INSERT INTO player_tx_mapping values(%s,\'%s\',\'%s\',\'%s\',%d,%d, NULL);",
		txid_str, table_id, req_identifier, msig_addr_nodes_str, tx_unspent, threshold_value);
	retval = bet_run_query(sql_query); // This is to update payin_tx in the players DB
	free((void *)txid_str);
	free((void *)msig_addr_nodes_str);

	memset(sql_query, 0x00, sql_query_size);
	msig_addr_nodes = cJSON_CreateArray();
	msig_addr_nodes = cJSON_GetObjectItem(argjson, "msig_addr_nodes");
	
	const char *msig_addr_nodes_unstr = unstringify(cJSON_Print(cJSON_GetObjectItem(argjson, "msig_addr_nodes")));
	txid_str = cJSON_Print(txid);
	if (txid_str == NULL || msig_addr_nodes_unstr == NULL) {
		dlg_error("Invalid txid or msig_addr_nodes for second query");
		if (txid_str) free((void *)txid_str);
		if (sql_query) free(sql_query);
		return ERR_ARGS_NULL;
	}

	required_size = snprintf(NULL, 0, "INSERT INTO c_tx_addr_mapping values(%s,\'%s\',%d,\'%s\',\'%s\',1,NULL);",
		txid_str, legacy_m_of_n_msig_addr, threshold_value, table_id, msig_addr_nodes_unstr) + 1;

	if (required_size > sql_query_size) {
		char *new_sql_query = realloc(sql_query, required_size);
		if (new_sql_query == NULL) {
			free((void *)txid_str);
			free(sql_query);
			return ERR_MEMORY_ALLOC;
		}
		sql_query = new_sql_query;
	}

	snprintf(sql_query, required_size, "INSERT INTO c_tx_addr_mapping values(%s,\'%s\',%d,\'%s\',\'%s\',1,NULL);",
		txid_str, legacy_m_of_n_msig_addr, threshold_value, table_id, msig_addr_nodes_unstr);
	free((void *)txid_str);

	payin_tx_insert_query = cJSON_CreateObject();
	cJSON_AddStringToObject(payin_tx_insert_query, "method", "lock_in_tx");
	cJSON_AddStringToObject(payin_tx_insert_query, "sql_query", sql_query);
	for (int32_t i = 0; i < cJSON_GetArraySize(msig_addr_nodes); i++) {
		bet_msg_cashier(payin_tx_insert_query,
				unstringify(cJSON_Print(cJSON_GetArrayItem(msig_addr_nodes, i))));
	}
	if (sql_query)
		free(sql_query);

	return retval;
}

static int32_t check_funds_for_poker(double table_stake)
{
	int32_t no_of_possible_moves = 20, retval = 0;
	double game_fee = 0, funds_needed = 0, balance = 0;

	game_fee = no_of_possible_moves * chips_tx_fee;

	funds_needed = table_stake + game_fee + chips_tx_fee;

	balance = chips_get_balance();

	dlg_info("table_stake ::%f\n, reserved_game_fee ::%f\n, funds_available::%f", table_stake, game_fee, balance);
	if (balance >= funds_needed) {
		retval = 1;
	}

	return retval;
}

static struct cJSON *add_tx_split_vouts(double amount, char *address)
{
	cJSON *vout_addresses = NULL;
	int no_of_split_tx = 20;

	vout_addresses = cJSON_CreateArray();
	if (address) {
		cJSON *payin_vout = cJSON_CreateObject();
		cJSON_AddStringToObject(payin_vout, "addr", address);
		cJSON_AddNumberToObject(payin_vout, "amount", amount);
		cJSON_AddItemToArray(vout_addresses, payin_vout);
	}
	for (int32_t i = 0; i < no_of_split_tx; i++) {
		cJSON *fee_vout = cJSON_CreateObject();
		cJSON_AddStringToObject(fee_vout, "addr", chips_get_new_address());
		cJSON_AddNumberToObject(fee_vout, "amount", chips_tx_fee);
		cJSON_AddItemToArray(vout_addresses, fee_vout);
	}
	return vout_addresses;
}

static void bet_player_check_dealer_gui_url(cJSON *argjson)
{
	if (0 == check_url(jstr(argjson, "gui_url"))) {
		if (0 == strlen(jstr(argjson, "gui_url"))) {
			dlg_warn("Dealer is not hosting the GUI");
		} else {
			dlg_warn("Dealer hosted GUI :: %s is not reachable", jstr(argjson, "gui_url"));
		}
		dlg_info("Player can use any of the GUI's hosted by cashiers to connect to backend");
		bet_display_cashier_hosted_gui();

	} else {
		dlg_warn("Dealer hosted GUI :: %s, using this you can connect to player backend and interact",
			 jstr(argjson, "gui_url"));
	}
}

static int32_t bet_do_player_checks(cJSON *argjson, struct privatebet_info *bet)
{
	int32_t retval = OK;
	if (jdouble(argjson, "dcv_commission") > max_allowed_dcv_commission) {
		dlg_warn(
			"Dealer set the commission %f%% which is more than max commission i.e %f%% set by player, so exiting",
			jdouble(argjson, "dcv_commission"), max_allowed_dcv_commission);
		retval = ERR_DCV_COMMISSION_MISMATCH;
		return retval;
	}

	if (0 == check_funds_for_poker(jdouble(argjson, "table_min_stake"))) {
		retval = ERR_CHIPS_INSUFFICIENT_FUNDS;
		return retval;
	}
	return retval;
}

static int32_t bet_player_initialize_table_params(cJSON *argjson, struct privatebet_info *bet)
{
	BB_in_chips = jdouble(argjson, "bb_in_chips");
	SB_in_chips = BB_in_chips / 2;
	table_stake_in_chips = table_stack_in_bb * BB_in_chips;
	if (table_stake_in_chips < jdouble(argjson, "table_min_stake")) {
		table_stake_in_chips = jdouble(argjson, "table_min_stake");
	}
	if (table_stake_in_chips > jdouble(argjson, "table_max_stake")) {
		table_stake_in_chips = jdouble(argjson, "table_max_stake");
	}
	max_players = jint(argjson, "max_players");
	chips_tx_fee = jdouble(argjson, "chips_tx_fee");
	legacy_m_of_n_msig_addr = (char *)malloc(strlen(jstr(argjson, "legacy_m_of_n_msig_addr")) + 1);
	memset(legacy_m_of_n_msig_addr, 0x00, strlen(jstr(argjson, "legacy_m_of_n_msig_addr")) + 1);
	strncpy(legacy_m_of_n_msig_addr, jstr(argjson, "legacy_m_of_n_msig_addr"),
		strlen(jstr(argjson, "legacy_m_of_n_msig_addr")));
	threshold_value = jint(argjson, "threshold_value");
	memset(table_id, 0x00, sizeof(table_id));
	strncpy(table_id, jstr(argjson, "table_id"), strlen(jstr(argjson, "table_id")));

	bet->maxplayers = max_players;
	bet->numplayers = max_players;
	return OK;
}

static struct cJSON *bet_player_make_payin_tx_data(cJSON *argjson, struct privatebet_info *bet)
{
	cJSON *payin_tx_data = NULL;

	payin_tx_data = cJSON_CreateObject();
	cJSON_AddStringToObject(payin_tx_data, "table_id", table_id);
	cJSON_AddStringToObject(payin_tx_data, "msig_addr_nodes",
				unstringify(cJSON_Print(cJSON_GetObjectItem(argjson, "msig_addr_nodes"))));
	cJSON_AddNumberToObject(payin_tx_data, "min_cashiers", threshold_value);
	cJSON_AddStringToObject(payin_tx_data, "player_id", req_identifier);
	cJSON_AddStringToObject(payin_tx_data, "dispute_addr", chips_get_new_address());
	cJSON_AddStringToObject(payin_tx_data, "msig_addr", legacy_m_of_n_msig_addr);
	cJSON_AddStringToObject(payin_tx_data, "tx_type", "payin");

	return payin_tx_data;
}

static int32_t bet_player_handle_stack_info_resp(cJSON *argjson, struct privatebet_info *bet)
{
	int32_t retval = OK, hex_data_len = 0;
	//double funds_available;
	char *hex_data = NULL;
	cJSON *tx_info = NULL, *txid = NULL, *payin_tx_data = NULL, *vout_addresses = NULL;

	bet_player_check_dealer_gui_url(argjson);

	retval = bet_do_player_checks(argjson, bet);
	if (retval != OK)
		return retval;

	bet_player_initialize_table_params(argjson, bet);

	/*
	funds_available = chips_get_balance() - chips_tx_fee;
	if (funds_available < jdouble(argjson, "table_min_stake")) {
		retval = ERR_CHIPS_INSUFFICIENT_FUNDS;
		return retval;
	}
	*/

	payin_tx_data = bet_player_make_payin_tx_data(argjson, bet);

	hex_data_len = 2 * strlen(cJSON_Print(payin_tx_data)) + 1;
	hex_data = calloc(hex_data_len, sizeof(char));
	str_to_hexstr(cJSON_Print(payin_tx_data), hex_data);

	/*
	dlg_info("funds_needed::%f", table_stake_in_chips);
	
	dlg_info("Will wait for a while till the tx's in mempool gets cleared");
	while (!chips_is_mempool_empty()) {
		sleep(2);
	}
	*/
	vout_addresses = add_tx_split_vouts(table_stake_in_chips, legacy_m_of_n_msig_addr);
	txid = chips_transfer_funds_with_data1(vout_addresses, hex_data);
	//txid = chips_transfer_funds_with_data(table_stake_in_chips, legacy_m_of_n_msig_addr, hex_data);
	if (txid == NULL) {
		retval = ERR_CHIPS_INVALID_TX;
		return retval;
	} else {
		retval = bet_store_game_info_details(cJSON_Print(txid), table_id);
	}
	dlg_info("tx id::%s", cJSON_Print(txid));
	memset(player_payin_txid, 0x00, sizeof(player_payin_txid));
	strcpy(player_payin_txid, cJSON_Print(txid));

	retval = bet_update_payin_tx_across_cashiers(argjson, txid);
	if (retval != OK) {
		dlg_error("Updating payin_tx across the cashier nodes or in the player DB got failed");
		retval = OK;
	}

	tx_info = cJSON_CreateObject();
	cJSON_AddStringToObject(tx_info, "method", "tx");
	cJSON_AddStringToObject(tx_info, "id", req_identifier);
	cJSON_AddStringToObject(tx_info, "chips_addr", chips_get_new_address());
	cJSON_AddItemToObject(tx_info, "tx_info", txid);
	dlg_info("Waiting for tx to confirm");
	while (chips_get_block_hash_from_txid(cJSON_Print(txid)) == NULL) {
		sleep(2);
	}
	dlg_info("TX ::%s got confirmed", cJSON_Print(txid));
	cJSON_AddNumberToObject(
		tx_info, "block_height",
		chips_get_block_height_from_block_hash(chips_get_block_hash_from_txid(cJSON_Print(txid))));
	// Nanomsg removed - no longer used
	retval = OK;

	if (hex_data)
		free(hex_data);
	return retval;
}

int32_t bet_player_stack_info_req(struct privatebet_info *bet)
{
	int32_t retval = OK;
	cJSON *stack_info_req = NULL;
	char rand_str[65] = { 0 };
	bits256 randval;

	stack_info_req = cJSON_CreateObject();
	cJSON_AddStringToObject(stack_info_req, "method", "stack_info_req");
	OS_randombytes(randval.bytes, sizeof(randval));
	bits256_str(rand_str, randval);
	strncpy(req_identifier, rand_str, sizeof(req_identifier));
	cJSON_AddStringToObject(stack_info_req, "id", rand_str);
	cJSON_AddStringToObject(stack_info_req, "chips_addr", chips_get_new_address());
	cJSON_AddNumberToObject(stack_info_req, "is_table_private", is_table_private);
	if (is_table_private) {
		cJSON_AddStringToObject(stack_info_req, "table_password", table_password);
	}
	// Nanomsg removed - no longer used
	retval = OK;

	return retval;
}

static int32_t bet_player_process_payout_tx(cJSON *argjson)
{
	char *sql_query = NULL;
	int32_t rc = OK;

	dlg_info("%s\n", cJSON_Print(argjson));
	sql_query = calloc(sql_query_size, sizeof(char));
	sprintf(sql_query, "UPDATE player_tx_mapping set status = 0,payout_tx_id = \'%s\' where table_id = \'%s\'",
		jstr(argjson, "tx_info"), jstr(argjson, "table_id"));
	rc = bet_run_query(sql_query);
	if (rc != SQLITE_OK)
		rc = ERR_SQL;
	if (sql_query)
		free(sql_query);
	return rc;
}

static int32_t bet_player_process_game_info(cJSON *argjson)
{
	int argc = 3, retval = OK;
	char **argv = NULL;
	char *sql_query = calloc(sql_query_size, sizeof(char));

	bet_alloc_args(argc, &argv);
	strcpy(argv[0], "player_game_state");
	sprintf(argv[1], "\'%s\'", table_id);
	sprintf(argv[2], "\'%s\'", cJSON_Print(cJSON_GetObjectItem(argjson, "game_state")));

	bet_make_insert_query(argc, argv, &sql_query);
	retval = bet_run_query(sql_query);
	if (retval != SQLITE_OK)
		retval = ERR_SQL;

	bet_dealloc_args(argc, &argv);
	if (sql_query)
		free(sql_query);
	return retval;
}

static void bet_update_seat_info(cJSON *argjson)
{
	cJSON *seats_info = NULL;

	seats_info = cJSON_CreateObject();
	cJSON_AddStringToObject(seats_info, "method", "seats");
	cJSON_AddItemToObject(seats_info, "seats", cJSON_GetObjectItem(argjson, "seats"));
	dlg_info("%s", cJSON_Print(seats_info));
	player_lws_write(seats_info);
}

void bet_handle_player_error(struct privatebet_info *bet, int32_t err_no)
{
	cJSON *publish_error = NULL;

	dlg_error("%s", bet_err_str(err_no));
	publish_error = cJSON_CreateObject();
	cJSON_AddStringToObject(publish_error, "method", "player_error");
	cJSON_AddNumberToObject(publish_error, "playerid", bet->myplayerid);
	cJSON_AddNumberToObject(publish_error, "err_no", err_no);
	// Nanomsg removed - no longer used
	if (0)
		exit(-1);

	switch (err_no) {
	case ERR_DECRYPTING_OWN_SHARE:
	case ERR_DECRYPTING_OTHER_SHARE:
	case ERR_CARD_RETRIEVING_USING_SS:
		dlg_info("This error can impact whole game...");
		break;
	case ERR_DEALER_TABLE_FULL:
		bet_raise_dispute(player_payin_txid);
	case ERR_LN_ADDRESS_TYPE_MISMATCH:
	case ERR_INVALID_POS:
	case ERR_CHIPS_INSUFFICIENT_FUNDS:
	case ERR_CHIPS_INVALID_TX:
	case ERR_PT_PLAYER_UNAUTHORIZED:
	case ERR_DCV_COMMISSION_MISMATCH:
	case ERR_INI_PARSING:
	case ERR_JSON_PARSING:
	case ERR_NNG_SEND:
	case ERR_NNG_BINDING:
	case ERR_PTHREAD_LAUNCHING:
	case ERR_PTHREAD_JOINING:
		exit(-1);
	default:
		dlg_error("The err_no :: %d is not handled by the backend player yet", err_no);
	}
}

int32_t bet_player_backend(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	char *method = NULL;
	char hexstr[65];

	if (strcmp(jstr(argjson, "method"), "reset") == 0) {
		reset_lock = 0;
		retval = bet_player_reset(bet, vars);
	}
	if (reset_lock == 1) {
		return retval;
	}
	if ((method = jstr(argjson, "method")) != 0) {
		dlg_info("recv :: %s", method);
		if (strcmp(method, "join_res") == 0) {
			bet_update_seat_info(argjson);
			if (strcmp(jstr(argjson, "req_identifier"), req_identifier) == 0) {
				bet_push_join_info(argjson);
				if (jint(argjson, "pos_status") == pos_on_table_empty) {
					retval = bet_client_join_res(argjson, bet, vars);
				} else {
					dlg_warn(
						"Player selected pos on the talbe is already taken, player need to select another pos to sit in");
				}
			}
		} else if (strcmp(method, "init") == 0) {
			if (jint(argjson, "peerid") == bet->myplayerid) {
				bet_player_blinds_info();
				dlg_info("myplayerid::%d::init::%s\n", bet->myplayerid, cJSON_Print(argjson));
				retval = bet_client_init(argjson, bet, vars);
			}
		} else if (strcmp(method, "init_d") == 0) {
			retval = bet_client_dcv_init(argjson, bet, vars);
		} else if (strcmp(method, "init_b") == 0) {
			retval = bet_client_bvv_init(argjson, bet, vars);
		} else if (strcmp(method, "turn") == 0) {
			if (jint(argjson, "playerid") == bet->myplayerid) {
				retval = bet_client_turn(argjson, bet, vars);
			}
		} else if (strcmp(method, "ask_share") == 0) {
			retval = bet_client_give_share(argjson, bet, vars);
		} else if (strcmp(method, "requestShare") == 0) {
			if (jint(argjson, "other_player") == bet->myplayerid) {
				retval = bet_client_give_share(argjson, bet, vars);
			}
		} else if (strcmp(method, "share_info") == 0) {
			if (jint(argjson, "to_playerid") == bet->myplayerid) {
				retval = bet_client_receive_share(argjson, bet, vars);
			}
		} else if (strcmp(method, "invoice") == 0) {
			// Lightning Network support removed - invoice payment no longer supported
			dlg_warn("Lightning Network invoice payment is no longer supported");
			retval = ERR_LN;
		} else if (strcmp(method, "bettingInvoice") == 0) {
			retval = bet_player_betting_invoice(argjson, bet, vars);
		} else if (strcmp(method, "winner") == 0) {
			retval = bet_player_winner(argjson, bet, vars);
		} else if (strcmp(method, "betting") == 0) {
			retval = bet_player_betting_statemachine(argjson, bet, vars);
		} else if (strcmp(method, "display_current_state") == 0) {
			retval = bet_display_current_state(argjson, bet, vars);
		} else if (strcmp(method, "dealer") == 0) {
			retval = bet_player_dealer_info(argjson, bet, vars);
		} else if (strcmp(method, "invoiceRequest_player") == 0) {
			retval = bet_player_create_invoice(argjson, bet, vars, bits256_str(hexstr, player_info.deckid));
		} else if (strcmp(method, "reset") == 0) {
			retval = bet_player_reset(bet, vars);
		} else if (strcmp(method, "seats") == 0) {
			// Lightning Network support removed - using chips balance instead
			cJSON_AddNumberToObject(argjson, "playerFunds", (int64_t)(chips_get_balance() * satoshis));
			player_lws_write(argjson);
		} else if (strcmp(method, "finalInfo") == 0) {
			player_lws_write(argjson);
		} else if (strcmp(method, "player_stakes_info") == 0) {
			cJSON *stakes = cJSON_GetObjectItem(argjson, "stakes");
			dlg_info("Player_stakes");
			for (int32_t i = 0; i < bet->maxplayers; i++) {
				vars->funds[i] = jinti(stakes, i);
    dlg_info("player::%d, stake::%.8f", i, vars->funds[i]);
			}
		} else if (strcmp(method, "signrawtransaction") == 0) {
			if (jint(argjson, "playerid") == bet->myplayerid) {
				cJSON *temp = cJSON_CreateObject();
				temp = chips_sign_raw_tx_with_wallet(jstr(argjson, "tx"));
				cJSON *signedTxInfo = cJSON_CreateObject();

				cJSON_AddStringToObject(signedTxInfo, "method", "signedrawtransaction");
				cJSON_AddStringToObject(signedTxInfo, "tx", jstr(temp, "hex"));
				cJSON_AddNumberToObject(signedTxInfo, "playerid", bet->myplayerid);
				// Nanomsg removed - no longer used
				retval = OK;
			}
		} else if (strcmp(method, "stack_info_resp") == 0) {
			if (strncmp(req_identifier, jstr(argjson, "id"), sizeof(req_identifier)) == 0) {
				retval = bet_player_handle_stack_info_resp(argjson, bet);
			}
		} else if (strcmp(method, "tx_status") == 0) {
			if (strcmp(req_identifier, jstr(argjson, "id")) == 0) {
				bet_player_table_info();
				vars->player_funds = jint(argjson, "player_funds");
				if (jint(argjson, "tx_validity") == OK) {
					dlg_info("Dealer verified the TX made by the player");
					if (backend_status == backend_ready) {
						/* 
						This snippet is added to handle the reset scenario after the initial hand got played. How this works is the backend status 
						for the player is set when the dealer verifies the tx as valid.
						It means when the backend is ready, for the next hand the GUI is expecting to push the reset info from the backend for this reason
						this snippet is added.
						*/
						cJSON *reset_info = cJSON_CreateObject();
						cJSON_AddStringToObject(reset_info, "method", "reset");
						player_lws_write(reset_info);
					}
					backend_status = backend_ready;
					cJSON *info = cJSON_CreateObject();
					cJSON_AddStringToObject(info, "method", "backend_status");
					cJSON_AddNumberToObject(info, "backend_status", backend_status);
					player_lws_write(info);
					cJSON *req_seats_info = NULL;
					req_seats_info = cJSON_CreateObject();
					cJSON_AddStringToObject(req_seats_info, "method", "req_seats_info");
					cJSON_AddStringToObject(req_seats_info, "req_identifier", req_identifier);
					dlg_info("Player requesting seats info from the dealer to join");
					// Nanomsg removed - no longer used
					retval = OK;

				} else {
					retval = ERR_CHIPS_INVALID_TX;
				}
			}
		} else if (strcmp(method, "payout_tx") == 0) {
			if (jstr(argjson, "tx_info")) {
				retval = bet_store_game_info_details(jstr(argjson, "tx_info"),
								     jstr(argjson, "table_id"));
				retval = bet_player_process_payout_tx(argjson);
			} else {
				dlg_warn("Error occured in payout_tx, so raising the dispute for payin_tx");
				bet_raise_dispute(player_payin_txid);
			}
		} else if (strcmp(method, "game_info") == 0) {
			retval = bet_player_process_game_info(argjson);
		} else if (strcmp(method, "dcv_state") == 0) {
			if (strncmp(req_identifier, jstr(argjson, "id"), sizeof(req_identifier)) == 0) {
				if (jint(argjson, "dcv_state") == 1) {
					dlg_warn("DCV which you trying to connect is full");
					dlg_info("%s\n", cJSON_Print(argjson));
					bet_player_reset(bet, vars);
					retval = ERR_DEALER_TABLE_FULL;
				}
			}
		} else if (strcmp(method, "tx_reverse") == 0) {
			if (strncmp(req_identifier, jstr(argjson, "id"), sizeof(req_identifier)) == 0) {
				dlg_warn(
					"The dealers table is already full, the payin_tx will be reversed using dispute resolution protocol::%s\n",
					cJSON_Print(argjson));
				bet_raise_dispute(player_payin_txid);
				retval = ERR_DEALER_TABLE_FULL;
			}
		} else if (strcmp(method, "seats_info_resp") == 0) {
			if (strcmp(req_identifier, jstr(argjson, "req_identifier")) == 0) {
				cJSON *seats_info = cJSON_CreateObject();
				cJSON_AddStringToObject(seats_info, "method", "seats");
				cJSON_AddItemToObject(seats_info, "seats", cJSON_GetObjectItem(argjson, "seats"));
				player_lws_write(seats_info);
				if ((backend_status == backend_ready) && (ws_connection_status == 0)) {
					dlg_info("Backend is ready, from GUI you can connect to backend and play...");
				}
			}
		} else if (strcmp(method, "is_player_active") == 0) {
			cJSON *active_info = NULL;
			active_info = cJSON_CreateObject();
			cJSON_AddStringToObject(active_info, "method", "player_active");
			cJSON_AddNumberToObject(active_info, "playerid", bet->myplayerid);
			cJSON_AddStringToObject(active_info, "req_identifier", req_identifier);
			// Nanomsg removed - no longer used
			retval = OK;
		} else if (strcmp(method, "active_player_info") == 0) {
			player_lws_write(argjson);
		} else if (strcmp(method, "game_abort") == 0) {
			dlg_warn("Player :: %d encounters the error ::%s, it has impact on game so exiting...",
				 jint(argjson, "playerid"), bet_err_str(jint(argjson, "err_no")));
			bet_raise_dispute(player_payin_txid);
			exit(-1);
		} else if (strcmp(method, "game_abort_player") == 0) {
			if (strcmp(req_identifier, jstr(argjson, "id")) == 0) {
				bet_handle_player_error(bet, jint(argjson, "err_no"));
				exit(-1);
			}
		} else {
			dlg_info("%s method is not handled in the backend\n", method);
		}
	}
	return retval;
}

void bet_player_backend_loop(void *_ptr)
{
	int32_t retval = OK;
	(void)_ptr; /* unused parameter */
	struct privatebet_info *bet = _ptr;

	retval = bet_player_stack_info_req(bet);
	if (retval != OK) {
		bet_handle_player_error(bet, retval);
	}
	// Nanomsg removed - communication now via websockets only
	// The player backend loop is handled through websocket callbacks
}

int32_t bet_player_reset(struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;

	player_joined = 0;
	no_of_shares = 0;
	no_of_player_cards = 0;
	for (int i = 0; i < bet->range; i++) {
		for (int j = 0; j < bet->numplayers; j++) {
			sharesflag[i][j] = 0;
		}
	}
	number_cards_drawn = 0;
	for (int i = 0; i < hand_size; i++) {
		player_card_matrix[i] = 0;
		player_card_values[i] = -1;
	}

	vars->pot = 0;
	for (int i = 0; i < bet->maxplayers; i++) {
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			vars->bet_actions[i][j] = 0;
			vars->betamount[i][j] = 0;
		}
	}

	memset(req_identifier, 0x00, sizeof(req_identifier));
	if (sitout_value == 0) {
		retval = bet_player_stack_info_req(
			bet); //sg777 commenting this to remove the auto start of the next hand
	} else {
		reset_lock = 1;
		dlg_info("The player is choosen sitout option, so has to wait until the ongoing hand to be finished");
	}
	return retval;
}

bits256 bet_get_deckid(int32_t player_id)
{
	return all_players_info[player_id].deckid;
}

void rest_push_cards(struct lws *wsi, cJSON *argjson, int32_t this_playerID)
{
	char *cards[52] = { "2C", "3C", "4C", "5C", "6C", "7C", "8C", "9C", "10C", "JC", "QC", "KC", "AC",
			    "2D", "3D", "4D", "5D", "6D", "7D", "8D", "9D", "10D", "JD", "QD", "KD", "AD",
			    "2H", "3H", "4H", "5H", "6H", "7H", "8H", "9H", "10H", "JH", "QH", "KH", "AH",
			    "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "10S", "JS", "QS", "KS", "AS" };

	cJSON *init_card_info = NULL, *hole_card_info = NULL, *init_info = NULL, *board_card_info = NULL;

	init_info = cJSON_CreateObject();
	cJSON_AddStringToObject(init_info, "method", "deal");

	init_card_info = cJSON_CreateObject();
	cJSON_AddNumberToObject(init_card_info, "dealer", 0);

	hole_card_info = cJSON_CreateArray();
	for (int32_t i = 0; ((i < no_of_hole_cards) && (i < all_number_cards_drawn[this_playerID])); i++) {
		cJSON_AddItemToArray(hole_card_info,
				     cJSON_CreateString(cards[all_player_card_values[this_playerID][i]]));
	}

	cJSON_AddItemToObject(init_card_info, "holecards", hole_card_info);

	board_card_info = cJSON_CreateArray();
	for (int32_t i = no_of_hole_cards; ((i < hand_size) && (i < all_number_cards_drawn[this_playerID])); i++) {
		cJSON_AddItemToArray(board_card_info,
				     cJSON_CreateString(cards[all_player_card_values[this_playerID][i]]));
	}

	cJSON_AddItemToObject(init_card_info, "board", board_card_info);

	cJSON_AddItemToObject(init_info, "deal", init_card_info);
	lws_write(wsi, (unsigned char *)cJSON_Print(init_info), strlen(cJSON_Print(init_info)), 0);
}

void rest_display_cards(cJSON *argjson, int32_t this_playerID)
{
	char *suit[NSUITS] = { "clubs", "diamonds", "hearts", "spades" };
	char *face[NFACES] = { "two",  "three", "four", "five",  "six",  "seven", "eight",
			       "nine", "ten",   "jack", "queen", "king", "ace" };

	char action_str[8][100] = { "", "small_blind", "big_blind", "check", "raise", "call", "allin", "fold" };
	cJSON *actions = NULL;
	int flag;

	dlg_info("******************** Player Cards ********************");
	dlg_info("Hole Cards:");
	for (int32_t i = 0; ((i < no_of_hole_cards) && (i < all_number_cards_drawn[this_playerID])); i++) {
		dlg_info("%s-->%s \t", suit[all_player_card_values[this_playerID][i] / 13],
			 face[all_player_card_values[this_playerID][i] % 13]);
	}

	flag = 1;
	for (int32_t i = no_of_hole_cards; ((i < hand_size) && (i < all_number_cards_drawn[this_playerID])); i++) {
		if (flag) {
			dlg_info("Community Cards:");
			flag = 0;
		}
		dlg_info("%s-->%s \t", suit[all_player_card_values[this_playerID][i] / 13],
			 face[all_player_card_values[this_playerID][i] % 13]);
	}

	dlg_info("******************** Betting done so far ********************");
	dlg_info("small_blind:%d, big_blind:%d", small_blind_amount, big_blind_amount);
	dlg_info("pot size:%d", jint(argjson, "pot"));
	actions = cJSON_GetObjectItem(argjson, "actions");
	int count = 0;
	flag = 1;
	for (int i = 0; ((i <= jint(argjson, "round")) && (flag)); i++) {
		dlg_info("Round:%d", i);
		for (int j = 0; ((j < BET_player[this_playerID]->maxplayers) && (flag)); j++) {
			if (jinti(actions, ((i * BET_player[this_playerID]->maxplayers) + j)) > 0)
				dlg_info("played id:%d, action: %s", j,
					 action_str[jinti(actions, ((i * BET_player[this_playerID]->maxplayers) + j))]);
			count++;
			if (count == cJSON_GetArraySize(actions))
				flag = 0;
		}
	}
}

cJSON *bet_get_available_dealers()
{
	cJSON *rqst_dealer_info = NULL, *cashier_response_info = NULL;
	cJSON *dealers_ip_info = NULL, *all_dealers_info = NULL;

	rqst_dealer_info = cJSON_CreateObject();
	cJSON_AddStringToObject(rqst_dealer_info, "method", "rqst_dealer_info");
	cJSON_AddStringToObject(rqst_dealer_info, "id", unique_id);
	all_dealers_info = cJSON_CreateArray();
	for (int32_t i = 0; i < no_of_notaries; i++) {
		if (notary_status[i] == 1) {
			cashier_response_info = bet_msg_cashier_with_response_id(rqst_dealer_info, notary_node_ips[i],
										 "rqst_dealer_info_response");
			if (cashier_response_info == NULL) {
				dlg_warn("No response from cashier :: %s", notary_node_ips[i]);
				continue;
			}
			dealers_ip_info = cJSON_CreateArray();
			dealers_ip_info = cJSON_GetObjectItem(cashier_response_info, "dealers_info");
			for (int32_t j = 0; j < cJSON_GetArraySize(dealers_ip_info); j++) {
				cJSON *temp = cJSON_GetArrayItem(dealers_ip_info, j);
				int flag = 1;
				for (int32_t k = 0; k < cJSON_GetArraySize(all_dealers_info); k++) {
					if (strcmp(jstr(cJSON_GetArrayItem(all_dealers_info, k), "ip"),
						   jstr(temp, "ip")) == 0) {
						flag = 0;
						break;
					}
				}
				if (flag)
					cJSON_AddItemToArray(all_dealers_info, temp);
			}
		}
	}
	return all_dealers_info;
}
