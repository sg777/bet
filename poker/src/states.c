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
#include "states.h"
#include "bet.h"
#include "client.h"
#include "common.h"
#include "host.h"
#include "network.h"
#include "payment.h"
#include "player.h"
#include "table.h"
#include "err.h"

char action_str[8][100] = { "", "small_blind", "big_blind", "check", "raise", "call", "allin", "fold" };

/***************************************************************
Here contains the functions which are specific to DCV
****************************************************************/

int32_t bet_initiate_statemachine(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *dealerInfo = NULL;

	vars->turni = 0;
	vars->round = 0;
	vars->pot = 0;
	vars->last_turn = 0;
	vars->last_raise = 0;
	for (int i = 0; i < bet->maxplayers; i++) {
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			vars->bet_actions[i][j] = 0;
			vars->betamount[i][j] = 0;
		}
	}
	dealerInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(dealerInfo, "method", "dealer");
	cJSON_AddNumberToObject(dealerInfo, "playerid", vars->dealer);

// Nanomsg removed - no longer used
	bet_push_dcv_to_gui(dealerInfo);

	return retval;
}

/**
 *
 * Function: BET_DCV_next_turn
 *
 * Parameters:
 *
 *		structure:  cJSON *argjson contains the information about
 *current active turn of the player structure:  struct privatebet_info *bet
 *contains deck and player information structure:  privatebet_vars *vars
 *contains the game information
 *
 * Return Value :
 *
 *		0 --> bet->maxplayers : It indicates there a valid turn exists
 *in the game. -1 : No valid turn exists
 *
 * Description:
 *
 *	  	This function identifies if there is any valid turn possible
 *during the game, if any valid turn exists it returns the index of the player
 *if not it returns -1
 *
 */

int32_t bet_dcv_next_turn(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars, int32_t *next_turn)
{
	int32_t maxamount = 0, retval = OK, players_left = 0;

	for (int i = 0; i < bet->maxplayers; i++) {
		if (vars->bet_actions[i][vars->round] == fold)
			players_left++;
	}
	players_left = bet->maxplayers - players_left;
	if (players_left < 2)
		return ERR_NO_TURN;

	for (int i = 0; i < bet->maxplayers; i++) {
		if (maxamount < vars->betamount[i][vars->round])
			maxamount = vars->betamount[i][vars->round];
	}
	for (int i = ((vars->turni + 1) % bet->maxplayers); (i != vars->turni); i = ((i + 1) % bet->maxplayers)) {
		if ((vars->bet_actions[i][vars->round] != fold) && (vars->bet_actions[i][vars->round] != allin) &&
		    (vars->funds[i] != 0)) {
			if (vars->bet_actions[i][vars->round] == 0) {
				for (int j = 0; j < bet->maxplayers; j++) {
					if ((i != j) &&
					    (((vars->bet_actions[j][vars->round] == 0) && (vars->funds[j] != 0)) ||
					     (vars->bet_actions[j][vars->round] != 0))) {
						*next_turn = i;
						return retval;
					}
				}
			} else if (/*(vars->bet_actions[i][vars->round] == 0) ||*/ (vars->bet_actions[i][vars->round] ==
										    small_blind) ||
				   (vars->bet_actions[i][vars->round] == big_blind) ||
				   (((vars->bet_actions[i][vars->round] == check) ||
				     (vars->bet_actions[i][vars->round] == call) ||
				     (vars->bet_actions[i][vars->round] == raise)) &&
				    (maxamount != vars->betamount[i][vars->round]))) {
				*next_turn = i;
				return retval;
			}
		}
	}
	return ERR_NO_TURN;
}

int32_t bet_dcv_round_betting(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	cJSON *roundBetting = NULL, *possibilities = NULL, *actions = NULL, *betAmounts = NULL;
	int maxamount = 0, retval = OK, players_left = 0, toCall = 0, toRaise = 0, totalBet = 0, next_turn = -1;

	if ((retval = bet_dcv_next_turn(argjson, bet, vars, &next_turn)) == ERR_NO_TURN) {
		for (int i = 0; i < bet->maxplayers; i++) {
			if (vars->bet_actions[i][vars->round] == fold) //|| (vars->bet_actions[i][vars->round]==allin))
				players_left++;
		}
		players_left = bet->maxplayers - players_left;

		vars->round += 1;
		vars->turni = vars->dealer;
		vars->last_raise = 0;

		if ((vars->round >= CARDS_MAXROUNDS) || (players_left < 2)) {
			vars->round -= 1;
			retval = bet_evaluate_hand(bet, vars);
			goto end;
		}

		retval = bet_dcv_turn(argjson, bet, vars);
		goto end;
	}

	players_left = 0;
	for (int i = 0; i < bet->maxplayers; i++) {
		if (vars->bet_actions[i][vars->round] == fold) //|| (vars->bet_actions[i][vars->round]==allin))
			players_left++;
	}
	players_left = bet->maxplayers - players_left;
	if (players_left < 2) {
		retval = bet_evaluate_hand(bet, vars);
		goto end;
	}

	//vars->turni = bet_dcv_next_turn(argjson, bet, vars, &next_turn);
	retval = bet_dcv_next_turn(argjson, bet, vars, &next_turn);
	if ((retval == OK) && (next_turn == -1)) {
		retval = ERR_INVALID_POS;
		return retval;
	}
	vars->last_turn = vars->turni;
	vars->turni = next_turn;
	roundBetting = cJSON_CreateObject();
	cJSON_AddStringToObject(roundBetting, "method", "betting");
	cJSON_AddStringToObject(roundBetting, "action", "round_betting");
	cJSON_AddNumberToObject(roundBetting, "playerid", vars->turni);
	cJSON_AddNumberToObject(roundBetting, "round", vars->round);
	cJSON_AddNumberToObject(roundBetting, "pot", vars->pot);
	/* */
	cJSON_AddItemToObject(roundBetting, "betAmounts", betAmounts = cJSON_CreateArray());
	for (int j = 0; j < bet->maxplayers; j++) {
		totalBet = 0;
		for (int i = 0; i <= vars->round; i++) {
			totalBet += vars->betamount[j][i];
		}
		cJSON_AddItemToArray(betAmounts, cJSON_CreateNumber(totalBet));
	}

	cJSON_AddItemToObject(roundBetting, "actions", actions = cJSON_CreateArray());
	for (int i = 0; i <= vars->round; i++) {
		for (int j = 0; j < bet->maxplayers; j++) {
			if (vars->bet_actions[j][i] > 0)
				cJSON_AddItemToArray(actions, cJSON_CreateNumber(vars->bet_actions[j][i]));
		}
	}

	cJSON_AddItemToObject(roundBetting, "possibilities", possibilities = cJSON_CreateArray());

	if ((vars->betamount[vars->last_turn][vars->round] == vars->betamount[vars->turni][vars->round]) &&
	    (vars->bet_actions[vars->turni][vars->round] == big_blind)) {
		toCall = vars->betamount[vars->turni][vars->round];
	} else {
		// raise, call, allin, fold
		int32_t max = 0;
		for (int32_t i = 0; i < bet->maxplayers; i++) {
			if (max < vars->betamount[i][vars->round])
				max = vars->betamount[i][vars->round];
		}
		toCall = max; //vars->betamount[vars->last_turn][vars->round];
	}

	if (vars->last_raise < big_blind_amount)
		toRaise = big_blind_amount;
	else
		toRaise = vars->last_raise;

	toRaise += toCall;

	cJSON_AddNumberToObject(roundBetting, "toCall", toCall);
	cJSON_AddNumberToObject(roundBetting, "minRaiseTo", toRaise);

	for (int i = 0; i < bet->maxplayers; i++) {
		if (maxamount < vars->betamount[i][vars->round]) {
			maxamount = vars->betamount[i][vars->round];
		}
	}

	if (maxamount > vars->betamount[vars->turni][vars->round]) {
		if (maxamount >= vars->funds[vars->turni]) {
			for (int i = allin; i <= fold; i++) {
				if (i == allin)
					cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(call));
				else
					cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
			}

		} else {
			int allin_flag = 1;
			for (int i = raise; i <= fold; i++) {
				if (i == raise) {
					for (int j = 0; j < bet->maxplayers; j++) {
						if ((j != vars->turni) && (vars->funds[j] != 0)) {
							cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
							break;
						}
					}
				} else if (i == call) {
					int totatBet = 0;
					for (int j = 0; j <= vars->round; j++)
						totatBet += vars->betamount[vars->turni][j];
					if (toCall == (totalBet + vars->funds[vars->turni])) {
						allin_flag = 0;
					}
					cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
				} else if (i == allin) {
					if (allin_flag == 1)
						cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
				} else
					cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
			}
		}

		// raise or fold or call
	} else if (maxamount == vars->betamount[vars->turni][vars->round]) {
		if (maxamount >= vars->funds[vars->turni]) {
			cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(check));
			for (int i = allin; i <= fold; i++)
				cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));

		} else {
			for (int i = check; i <= fold; i++) {
				if (i != call)
					cJSON_AddItemToArray(possibilities, cJSON_CreateNumber(i));
			}
		}
		// raise or fold or call or check
	}

	cJSON_AddNumberToObject(roundBetting, "min_amount", (maxamount - vars->betamount[vars->turni][vars->round]));

// Nanomsg removed - no longer used

end:
	return retval;
}

int32_t bet_dcv_round_betting_response(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid, round, bet_amount = 0, players_left = 0, min_amount = 0;
	char *action = NULL;
	cJSON *playerFunds = NULL;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");
	bet_amount = jint(argjson, "invoice_amount");
	min_amount = jint(argjson, "min_amount");

	vars->betamount[playerid][round] += bet_amount;
	vars->pot += bet_amount;
	vars->funds[playerid] -= bet_amount;

	cJSON_AddNumberToObject(argjson, "pot", vars->pot);
	cJSON_AddItemToObject(argjson, "player_funds", playerFunds = cJSON_CreateArray());
	for (int i = 0; i < bet->maxplayers; i++) {
		cJSON_AddItemToArray(playerFunds, cJSON_CreateNumber(vars->funds[i]));
	}

	if ((action = jstr(argjson, "action")) != NULL) {
		if (strcmp(action, "check") == 0) {
			vars->bet_actions[playerid][round] = check;
		} else if (strcmp(action, "call") == 0) {
			vars->bet_actions[playerid][round] = call;
		} else if (strcmp(action, "raise") == 0) {
			vars->bet_actions[playerid][round] = raise;
			vars->last_raise = bet_amount - min_amount;
		} else if (strcmp(action, "allin") == 0) {
			vars->bet_actions[playerid][round] = allin;
		} else if (strcmp(action, "fold") == 0) {
			vars->bet_actions[playerid][round] = fold;
			for (int32_t i = round; i < CARDS_MAXROUNDS; i++) {
				vars->bet_actions[playerid][i] = fold;
			}
		}

		for (int i = 0; i < bet->maxplayers; i++) {
			if (vars->bet_actions[i][round] == fold)
				players_left++;
		}
		players_left = bet->maxplayers - players_left;
		if (players_left < 2) {
			for (int i = 0; i < bet->maxplayers; i++) {
				for (int j = vars->round; j < CARDS_MAXROUNDS; j++) {
					vars->bet_actions[i][j] = vars->bet_actions[i][round]; // check
				}
			}
		}
	}
	retval = bet_dcv_round_betting(argjson, bet, vars);

	return retval;
}

int32_t bet_dcv_big_blind_bet(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int retval = OK, playerid, round;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");

	vars->big_blind = jint(argjson, "amount");
	vars->bet_actions[playerid][round] = big_blind;
	vars->betamount[playerid][round] = vars->big_blind;
	vars->pot += vars->big_blind;
	vars->funds[playerid] -= vars->big_blind;

	retval = bet_dcv_round_betting(argjson, bet, vars);

	return retval;
}

int32_t bet_dcv_big_blind(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *big_blind_info = NULL;

	vars->last_turn = vars->turni;
	vars->turni = (vars->turni + 1) % bet->maxplayers;
	big_blind_info = cJSON_CreateObject();
	cJSON_AddStringToObject(big_blind_info, "method", "betting");
	cJSON_AddStringToObject(big_blind_info, "action", "big_blind");
	cJSON_AddNumberToObject(big_blind_info, "playerid", vars->turni);
	cJSON_AddNumberToObject(big_blind_info, "min_amount", (vars->small_blind * 2));
	cJSON_AddNumberToObject(big_blind_info, "pot", vars->pot);

// Nanomsg removed - no longer used
	return retval;
}

int32_t bet_dcv_small_blind_bet(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid, round;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");

	vars->small_blind = jint(argjson, "amount");
	vars->bet_actions[playerid][round] = small_blind;
	vars->betamount[playerid][round] = vars->small_blind;
	vars->pot += vars->small_blind;
	vars->funds[playerid] -= vars->small_blind;

	retval = bet_dcv_big_blind(argjson, bet, vars);
	return retval;
}

int32_t bet_dcv_small_blind(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	cJSON *smallBlindInfo = NULL;

	vars->last_turn = vars->dealer;
	vars->turni = (vars->dealer) % bet->maxplayers; // vars->dealer+1 is removed since
	// dealer is the one who does small_blind

	smallBlindInfo = cJSON_CreateObject();
	cJSON_AddStringToObject(smallBlindInfo, "method", "betting");
	cJSON_AddStringToObject(smallBlindInfo, "action", "small_blind");
	cJSON_AddNumberToObject(smallBlindInfo, "playerid", vars->turni);
	cJSON_AddNumberToObject(smallBlindInfo, "round", vars->round);
	cJSON_AddNumberToObject(smallBlindInfo, "pot", vars->pot);
// Nanomsg removed - no longer used
	return retval;
}
/***************************************************************
Here contains the functions which are common across all the nodes
****************************************************************/

int32_t bet_player_betting_statemachine(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;
	char *action = NULL;

	if ((action = jstr(argjson, "action")) != 0) {
		if (strcmp(action, "small_blind") == 0) {
			if (jint(argjson, "playerid") == bet->myplayerid) {
				display_cards();
				retval = bet_player_small_blind(argjson, bet, vars);
			}
		} else if (strcmp(action, "big_blind") == 0) {
			if (jint(argjson, "playerid") == bet->myplayerid) {
				display_cards();
				retval = bet_player_big_blind(argjson, bet, vars);
			}
		} else if (strcmp(action, "small_blind_bet") == 0) {
			if (bet->myplayerid == -2) {
				bet_relay(argjson, bet, vars);
				retval = bet_dcv_small_blind_bet(argjson, bet, vars);
			} else {
				retval = bet_player_small_blind_bet(argjson, bet, vars);
			}
		} else if (strcmp(action, "big_blind_bet") == 0) {
			if (bet->myplayerid == -2) {
				bet_relay(argjson, bet, vars);
				retval = bet_dcv_big_blind_bet(argjson, bet, vars);
			} else {
				retval = bet_player_big_blind_bet(argjson, bet, vars);
			}

		} else if (strcmp(action, "round_betting") == 0) {
			display_cards();

			cJSON *player_funds = NULL;
			cJSON_AddItemToObject(argjson, "player_funds", player_funds = cJSON_CreateArray());

			for (int i = 0; i < bet->maxplayers; i++) {
				int totalBet = 0;
				for (int j = 0; j <= jint(argjson, "round"); j++) // for(int j=0;j<=vars->round;j++)
				{
					totalBet += vars->betamount[i][j];
				}
				cJSON_AddItemToArray(player_funds, cJSON_CreateNumber(vars->funds[i] - totalBet));
			}
			player_lws_write(argjson);
		} else if ((strcmp(action, "check") == 0) || (strcmp(action, "call") == 0) ||
			   (strcmp(action, "raise") == 0) || (strcmp(action, "fold") == 0) ||
			   (strcmp(action, "allin") == 0)) {
			if (bet->myplayerid == -2) {
				// Nanomsg removed - no longer used
				retval = OK;
				if (retval != OK)
					goto end;
				retval = bet_dcv_round_betting_response(argjson, bet, vars);
			} else {
				retval = bet_player_round_betting_response(argjson, bet, vars);
			}
		}
	}
end:
	return retval;
}

int32_t bet_display_current_state(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK;

	dlg_info("small_blind:%lld table chips", (long long)vars->small_blind);
	dlg_info("big_blind:%lld table chips", (long long)vars->big_blind);

	dlg_info("Display Actions:");

	for (int j = 0; j < CARDS_MAXROUNDS; j++) {
		dlg_info("Round:%d\n", j);
		for (int i = 0; i < bet->maxplayers; i++) {
			if (vars->bet_actions[i][j] == small_blind) {
				dlg_info("small blind ");
			} else if (vars->bet_actions[i][j] == big_blind) {
				dlg_info("big blind ");
			} else if (vars->bet_actions[i][j] == check) {
				dlg_info("raise ");
			} else if (vars->bet_actions[i][j] == raise) {
				dlg_info("check ");
			} else if (vars->bet_actions[i][j] == call) {
				dlg_info("call ");
			} else if (vars->bet_actions[i][j] == fold) {
				dlg_info("fold ");
			}
			dlg_info("%lld table chips ", (long long)vars->betamount[i][j]);
		}
	}

	return retval;
}

/***************************************************************
Here contains the functions which are specific to players and BVV
****************************************************************/
int32_t bet_player_small_blind_bet(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid, round;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");

	vars->turni = (vars->turni + 1) % bet->maxplayers;
	vars->small_blind = jint(argjson, "amount");
	vars->bet_actions[playerid][round] = small_blind;
	vars->betamount[playerid][round] = vars->small_blind;
	vars->pot += vars->small_blind;

	return retval;
}

int32_t bet_player_big_blind_bet(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int retval = OK, playerid, round;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");

	vars->turni = (vars->turni + 1) % bet->maxplayers;
	vars->big_blind = jint(argjson, "amount");
	vars->bet_actions[playerid][round] = big_blind;
	vars->betamount[playerid][round] = vars->big_blind;
	vars->pot += vars->big_blind;

	return retval;
}

int32_t bet_player_dealer_info(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int retval = OK;
	cJSON *dealerReady = NULL;

	vars->dealer = jint(argjson, "playerid");
	vars->turni = vars->dealer;
	vars->pot = 0;

	for (int i = 0; i < bet->maxplayers; i++) {
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			vars->bet_actions[i][j] = 0;
		}
	}
	for (int i = 0; i < bet->maxplayers; i++) {
		for (int j = 0; j < CARDS_MAXROUNDS; j++) {
			vars->betamount[i][j] = 0;
		}
	}

	if (vars->dealer == bet->myplayerid) {
		dlg_info("This player is next to dealer: %d\n", bet->myplayerid);
		dealerReady = cJSON_CreateObject();
		cJSON_AddStringToObject(dealerReady, "method", "dealer_ready");
// Nanomsg removed - no longer used
	}
	bet_push_client(argjson);
	return retval;
}

int32_t bet_player_small_blind(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t amount, retval = OK;
	cJSON *small_blind_info = NULL;

	amount = small_blind_amount;
	vars->player_funds -= amount;
	vars->betamount[bet->myplayerid][vars->round] = vars->betamount[bet->myplayerid][vars->round] + amount;

	// Lightning Network support removed - always use blockchain logging
	retval = bet_player_log_bet_info(argjson, bet, amount, small_blind);
	if (retval != OK) {
		dlg_error("%s", bet_err_str(retval));
	}

	small_blind_info = cJSON_CreateObject();
	cJSON_AddStringToObject(small_blind_info, "method", "betting");
	cJSON_AddStringToObject(small_blind_info, "action", "small_blind_bet");
	cJSON_AddNumberToObject(small_blind_info, "amount", amount);
	cJSON_AddNumberToObject(small_blind_info, "playerid", jint(argjson, "playerid"));
	cJSON_AddNumberToObject(small_blind_info, "round", jint(argjson, "round"));

// Nanomsg removed - no longer used
	player_lws_write(small_blind_info);

	return retval;
}

int32_t bet_player_big_blind(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	cJSON *big_blind_info = NULL;
	int32_t amount, retval = OK;

	amount = big_blind_amount;
	vars->player_funds -= amount;
	vars->betamount[bet->myplayerid][vars->round] = vars->betamount[bet->myplayerid][vars->round] + amount;

	// Lightning Network support removed - always use blockchain logging
	retval = bet_player_log_bet_info(argjson, bet, amount, big_blind);
	if (retval != OK) {
		dlg_error("%s", bet_err_str(retval));
	}
	big_blind_info = cJSON_CreateObject();
	cJSON_AddStringToObject(big_blind_info, "method", "betting");
	cJSON_AddStringToObject(big_blind_info, "action", "big_blind_bet");
	cJSON_AddNumberToObject(big_blind_info, "amount", amount);
	cJSON_AddNumberToObject(big_blind_info, "playerid", jint(argjson, "playerid"));
	cJSON_AddNumberToObject(big_blind_info, "round", jint(argjson, "round"));

// Nanomsg removed - no longer used
	player_lws_write(big_blind_info);

	return retval;
}

/* Handle a betting action that arrived from the GUI (--gui mode).
 *
 * The GUI sends a message of shape:
 *   {"method":"betting","action":"round_betting","playerid":N,
 *    "pot":P,"toCall":T,"minRaiseTo":M,"player_funds":[..],
 *    "possibilities":[CODE],"bet_amount":A}
 *
 * "possibilities" is overloaded - in the dealer->GUI direction it is the
 * list of legal actions, in the GUI->backend direction the GUI puts the
 * single chosen action code (matching enum action_type) in slot 0.
 *
 * All chip amounts (pot, toCall, minRaiseTo, player_funds, bet_amount)
 * are integer table chips (post chip-scaling refactor). The amount we
 * commit on this turn depends on the action:
 *   raise            -> bet_amount    (absolute raise-to value)
 *   call             -> toCall        (capped at remaining funds)
 *   allin            -> player_funds  (everything left)
 *   check / fold     -> 0
 *   small/big_blind  -> bet_amount    (rare: GUI-driven blinds)
 *
 * The action is written to the player's Verus identity under
 * P_BETTING_ACTION_KEY via player_write_betting_action(). The dealer
 * polls that key and updates dcv_vars + the betting_state CMM.
 *
 * This is the same write path --cli and --auto already use from
 * player_handle_betting() in player.c. The previous implementation
 * called bet_player_log_bet_info() (a dead nanomsg/multisig-era code
 * path) which always failed with "Address NULL", swallowed the error,
 * and never wrote anything on chain - causing the dealer to time out
 * and auto-fold the GUI player every hand.
 */
int32_t bet_player_round_betting(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	cJSON *possibilities = NULL;
	int32_t retval = OK, playerid, round, action_code;
	int64_t amount = 0, to_call;
	const char *action_name = NULL;

	(void)bet;  /* legacy multisig path removed - bet no longer needed */

	playerid = jint(argjson, "playerid");

	/* round MUST come from the dealer's T_BETTING_STATE_KEY (the
	 * authoritative round counter), NOT from the GUI message.
	 * gui_build_betting_round() in gui.c does not include "round" in the
	 * payload it sends to the GUI, so jint(argjson, "round") returned 0
	 * by default for every GUI-driven action. Pre-flop happened to work
	 * by accident because the dealer was also on round=0; on every
	 * post-flop round the action got stamped round=0 while the dealer
	 * expected round>=1, so the dealer's verus_poll_player_action()
	 * dropped it as a stale-round action and auto-folded the GUI player
	 * after its 121 s timeout.
	 *
	 * Same bug class as the comment in player.c::player_write_betting_action;
	 * the fix was applied there for the CLI/auto path but not extended to
	 * the GUI path here. By re-reading T_BETTING_STATE_KEY we pull the
	 * round the dealer is currently waiting for - which is exactly the
	 * round this action belongs to, since the dealer has not yet
	 * advanced (it is still waiting for our action). */
	{
		cJSON *bs = player_read_betting_state(player_config.table_id);
		round = bs ? jint(bs, "round") : 0;
		if (bs)
			cJSON_Delete(bs);
	}

	possibilities = cJSON_GetObjectItem(argjson, "possibilities");
	if (!possibilities || cJSON_GetArraySize(possibilities) != 1) {
		dlg_error("[GUI] betting message has no chosen action in possibilities[]");
		return ERR_ARGS_NULL;
	}
	action_code = jinti(possibilities, 0);
	if (action_code < small_blind || action_code > fold) {
		dlg_error("[GUI] invalid action code %d (expected %d..%d)",
			  action_code, small_blind, fold);
		return ERR_ARGS_NULL;
	}
	action_name = action_str[action_code];

	to_call = (int64_t)jint(argjson, "toCall");

	switch (action_code) {
	case raise:
	case small_blind:
	case big_blind:
		amount = (int64_t)jint(argjson, "bet_amount");
		break;
	case call:
		amount = (to_call < vars->player_funds) ? to_call : vars->player_funds;
		break;
	case allin:
		amount = vars->player_funds;
		break;
	case check:
	case fold:
	default:
		amount = 0;
		break;
	}

	/* Mirror in local vars so subsequent dealer broadcasts arrive at a
	 * consistent local view. Dealer is the source of truth and will
	 * rebroadcast authoritative state via the betting_state CMM. */
	vars->bet_actions[playerid][round] = action_code;
	vars->betamount[playerid][round] += amount;
	vars->player_funds -= amount;

	dlg_info("[GUI->VRSC] action=%s amount=%lld table chips (%.4f CHIPS)",
		 action_name, (long long)amount, table_chips_to_chips(amount));

	retval = player_write_betting_action(player_config.table_id, action_name, amount, round);
	if (retval != OK) {
		dlg_error("[GUI] failed to write betting action to Verus ID: %s",
			  bet_err_str(retval));
	}

	return retval;
}

int32_t bet_player_round_betting_response(cJSON *argjson, struct privatebet_info *bet, struct privatebet_vars *vars)
{
	int32_t retval = OK, playerid, round, invoice_amount = 0;
	char *action = NULL;
	cJSON *playerFunds = NULL;

	playerid = jint(argjson, "playerid");
	round = jint(argjson, "round");
	invoice_amount = jint(argjson, "invoice_amount");

	if (bet->myplayerid != playerid)
		vars->betamount[playerid][round] += invoice_amount;

	vars->pot += invoice_amount;

	cJSON_AddNumberToObject(argjson, "pot", vars->pot);
	cJSON_AddItemToObject(argjson, "player_funds", playerFunds = cJSON_CreateArray());
	for (int i = 0; i < bet->maxplayers; i++) {
		int totalBet = 0;
		for (int j = 0; j <= vars->round; j++) {
			totalBet += vars->betamount[i][j];
		}
		cJSON_AddItemToArray(playerFunds, cJSON_CreateNumber(vars->funds[i] - totalBet));
	}

	if ((action = jstr(argjson, "action")) != NULL) {
		if (strcmp(action, "check") == 0) {
			vars->bet_actions[playerid][round] = check;
		} else if (strcmp(action, "call") == 0) {
			vars->bet_actions[playerid][round] = call;
		} else if (strcmp(action, "raise") == 0) {
			vars->bet_actions[playerid][round] = raise;
		} else if (strcmp(action, "fold") == 0) {
			vars->bet_actions[playerid][round] = fold;
		} else if (strcmp(action, "allin") == 0) {
			vars->bet_actions[playerid][round] = allin;
		}
	}
	bet_push_client(argjson);

	return retval;
}
