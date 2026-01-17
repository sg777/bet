#include "log.h"
#include <dlg/dlg.h>
#include <dlg/output.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define LOG_DIR "/root/.bet/logs"
#define LOG_PATH_MAX 512

static FILE *g_log_file = NULL;
static bool g_terminal_enabled = true;
static char g_log_path[LOG_PATH_MAX] = {0};

// Style definitions for file output (no colors)
static const struct dlg_style file_styles[] = {
	{dlg_text_style_none, dlg_color_none, dlg_color_none}, // trace
	{dlg_text_style_none, dlg_color_none, dlg_color_none}, // debug
	{dlg_text_style_none, dlg_color_none, dlg_color_none}, // info
	{dlg_text_style_none, dlg_color_none, dlg_color_none}, // warn
	{dlg_text_style_none, dlg_color_none, dlg_color_none}, // error
	{dlg_text_style_none, dlg_color_none, dlg_color_none}  // fatal
};

// Custom log handler - writes to both file and terminal
static void bet_log_handler(const struct dlg_origin *origin, const char *string, void *data)
{
	(void)data;
	
	// Write to log file (no colors/styles)
	if (g_log_file) {
		// Add timestamp
		time_t now = time(NULL);
		struct tm *tm_info = localtime(&now);
		char timestamp[32];
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
		
		// Format: [timestamp] [level] [file:func:line] message
		const char *level_str = "???";
		switch (origin->level) {
			case dlg_level_trace: level_str = "TRC"; break;
			case dlg_level_debug: level_str = "DBG"; break;
			case dlg_level_info:  level_str = "INF"; break;
			case dlg_level_warn:  level_str = "WRN"; break;
			case dlg_level_error: level_str = "ERR"; break;
			case dlg_level_fatal: level_str = "FTL"; break;
			default: break;
		}
		
		fprintf(g_log_file, "[%s] [%s] [%s:%s:%u] %s\n",
			timestamp, level_str,
			origin->file ? origin->file : "?",
			origin->func ? origin->func : "?",
			origin->line,
			string);
		fflush(g_log_file);
	}
	
	// Write to terminal if enabled
	if (g_terminal_enabled) {
		dlg_default_output(origin, string, NULL);
	}
}

void bet_log_init(const char *node_id)
{
	// Check for quiet mode environment variable
	const char *quiet = getenv("BET_LOG_QUIET");
	if (quiet && (strcmp(quiet, "1") == 0 || strcmp(quiet, "true") == 0)) {
		g_terminal_enabled = false;
	}
	
	// Create log directory if it doesn't exist
	struct stat st = {0};
	if (stat(LOG_DIR, &st) == -1) {
		mkdir(LOG_DIR, 0755);
	}
	
	// Build log file path
	const char *name = node_id ? node_id : "debug";
	snprintf(g_log_path, sizeof(g_log_path), "%s/%s.log", LOG_DIR, name);
	
	// Open log file (append mode)
	g_log_file = fopen(g_log_path, "a");
	if (!g_log_file) {
		fprintf(stderr, "Warning: Could not open log file: %s\n", g_log_path);
	} else {
		// Write session start marker
		time_t now = time(NULL);
		fprintf(g_log_file, "\n========== Session started: %s", ctime(&now));
		fflush(g_log_file);
	}
	
	// Set our custom handler
	dlg_set_handler(bet_log_handler, NULL);
}

void bet_log_cleanup(void)
{
	if (g_log_file) {
		time_t now = time(NULL);
		fprintf(g_log_file, "========== Session ended: %s\n", ctime(&now));
		fclose(g_log_file);
		g_log_file = NULL;
	}
}

void bet_log_set_terminal(bool enabled)
{
	g_terminal_enabled = enabled;
}

const char *bet_log_get_path(void)
{
	return g_log_path[0] ? g_log_path : NULL;
}
