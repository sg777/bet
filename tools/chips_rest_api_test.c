/******************************************************************************
 * Simple C program to test CHIPS REST API via JSON-RPC
 * 
 * This demonstrates how to call CHIPS RPC methods using HTTP/JSON-RPC
 * instead of CLI commands.
 * 
 * Compilation:
 *   cd tools && make -f Makefile.test
 * 
 * Usage:
 *   ./chips_rest_api_test [rpc_url] [rpc_user] [rpc_password]
 * 
 * Example:
 *   ./chips_rest_api_test http://127.0.0.1:57776 chipsuser123 chipspass456
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// Structure to hold HTTP response data
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

/**
 * Make JSON-RPC request to CHIPS node
 * 
 * @param url RPC endpoint URL (e.g., http://127.0.0.1:57776)
 * @param rpc_user RPC username
 * @param rpc_password RPC password
 * @param method RPC method name (e.g., "getblockchaininfo")
 * @param params JSON array string of parameters (e.g., "[]" for no params)
 * @return JSON response string (caller must free), NULL on error
 */
char *chips_rpc_request(const char *url, const char *rpc_user, const char *rpc_password,
                        const char *method, const char *params)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        free(chunk.memory);
        return NULL;
    }

    // Build JSON-RPC request payload manually
    char json_payload[1024];
    snprintf(json_payload, sizeof(json_payload),
        "{\"jsonrpc\":\"1.0\",\"id\":\"1\",\"method\":\"%s\",\"params\":%s}",
        method, params);

    // Set up curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    
    // Set authentication
    char userpwd[512];
    snprintf(userpwd, sizeof(userpwd), "%s:%s", 
             rpc_user ? rpc_user : "", 
             rpc_password ? rpc_password : "");
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
    
    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform the request
    res = curl_easy_perform(curl);

    curl_slist_free_all(headers);

    // Check for errors
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return NULL;
    }

    // Get HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);

    if (response_code != 200) {
        fprintf(stderr, "HTTP error: %ld\n", response_code);
        if (chunk.memory) {
            fprintf(stderr, "Response: %s\n", chunk.memory);
        }
        free(chunk.memory);
        return NULL;
    }

    return chunk.memory;
}

int main(int argc, char *argv[])
{
    const char *rpc_url = "http://127.0.0.1:57776";
    const char *rpc_user = NULL;
    const char *rpc_password = NULL;

    // Parse command line arguments
    if (argc >= 2) {
        rpc_url = argv[1];
    }
    if (argc >= 3) {
        rpc_user = argv[2];
    }
    if (argc >= 4) {
        rpc_password = argv[3];
    }

    // Default values if not provided (CHIPS default RPC port is 57776)
    if (!rpc_user) {
        rpc_user = "chipsuser";
        printf("Warning: Using default RPC user 'chipsuser'. Set via command line argument.\n");
    }
    if (!rpc_password) {
        rpc_password = "chipspass";
        printf("Warning: Using default RPC password 'chipspass'. Set via command line argument.\n");
    }

    printf("Connecting to CHIPS RPC endpoint: %s\n", rpc_url);
    printf("RPC User: %s\n\n", rpc_user);

    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Make RPC call to getblockchaininfo
    printf("Calling getblockchaininfo...\n\n");
    char *response = chips_rpc_request(rpc_url, rpc_user, rpc_password, "getblockchaininfo", "[]");

    if (!response) {
        fprintf(stderr, "Failed to get blockchain info\n");
        curl_global_cleanup();
        return 1;
    }

    printf("=== CHIPS Blockchain Info ===\n%s\n", response);

    free(response);
    curl_global_cleanup();

    return 0;
}
