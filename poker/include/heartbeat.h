#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include "bet.h"

void bet_dcv_heartbeat_loop(void *_ptr);
void bet_dcv_update_player_status(cJSON *argjson);
void bet_dcv_heartbeat_thread(struct privatebet_info *bet);

#endif /* HEARTBEAT_H */
