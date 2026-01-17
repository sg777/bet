#ifndef BET_LOG_H
#define BET_LOG_H

#include <stdbool.h>

/**
 * Initialize logging system
 * @param node_id - Node identifier for log file name (p1, d1, cashier, etc.)
 *                  If NULL, uses "debug" as the log file name
 * 
 * Creates log file at /root/.bet/logs/<node_id>.log
 * Sets up dual output: terminal + file
 * 
 * Environment variables:
 *   BET_LOG_QUIET=1  - Suppress terminal output, only log to file
 */
void bet_log_init(const char *node_id);

/**
 * Close the log file and cleanup
 */
void bet_log_cleanup(void);

/**
 * Enable or disable terminal output
 * @param enabled - true to enable terminal output, false to suppress
 */
void bet_log_set_terminal(bool enabled);

/**
 * Get current log file path
 * @return Path to the current log file, or NULL if not initialized
 */
const char *bet_log_get_path(void);

#endif // BET_LOG_H
