#ifndef ERR_H
#define ERR_H

#include <stdio.h>
#include <stdint.h>

#include "../external/dlg/include/dlg/dlg.h"

/******************************************************************************
All the errors that come across in bet are defined here. The error numbers are assigned with the following criteria
* Error No's 1-32 reserved for bet gameplay
* Error No's 33-48 reserved for CHIPS and LN related issues
* All other erros numbered from 49 and so on with the spacing for new errors 
******************************************************************************/
#define MAX_ERR_NO 128
//Bet Gameplay Errors
#define OK 0
#define ERR_RESERVED 1
#define ERR_DECRYPTING_OWN_SHARE 2
#define ERR_DECRYPTING_OTHER_SHARE 3
#define ERR_CARD_RETRIEVING_USING_SS 4
#define ERR_PT_PLAYER_UNAUTHORIZED 5
#define ERR_NOT_ENOUGH_PLAYERS 6
#define ERR_ALL_CARDS_DRAWN 7
#define ERR_DCV_COMMISSION_MISMATCH 8
#define ERR_DEALER_TABLE_FULL 9
#define ERR_INVALID_POS 10
#define ERR_NO_TURN 11
//Chips LN Errors
#define ERR_CHIPS_TX_SIGNING 32
#define ERR_CHIPS_INVALID_TX 33
#define ERR_LN 34
#define ERR_LN_CHANNEL_ESTABLISHMENT 35
#define ERR_LN_IS_NOT_OVER_TOR 36
#define ERR_CHIPS_INSUFFICIENT_FUNDS 37
#define ERR_LN_INSUFFICIENT_FUNDS 38
#define ERR_CHIPS_TX_FAILED 39
#define ERR_LN_CHANNEL_FUNDING_FAILED 40
#define ERR_LN_PAY 41
#define ERR_LN_ADDRESS_TYPE_MISMATCH 42

//Parsing Errors
#define ERR_INI_PARSING 49
#define ERR_JSON_PARSING 50

//NNG Related Errors
#define ERR_NNG_SEND 51
#define ERR_NNG_BINDING 52

//PTHREAD Errors
#define ERR_PTHREAD_LAUNCHING 53
#define ERR_PTHREAD_JOINING 54

//SQLITE Errors
#define ERR_SQL 55

const char *bet_err_str(int32_t err_no);
#endif