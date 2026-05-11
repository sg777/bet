# Code Refactoring Samples - Before & After

## Issue 1: Buffer Overflow Risk - Unsafe strcpy()

### BEFORE (cashier.c:262):
```c
cashier_node_ips[i] = calloc(ip_length, sizeof(char));
strcpy(cashier_node_ips[i], unstringify(cJSON_Print(cJSON_GetArrayItem(msig_addr_nodes, i))));
```
**Problems:**
- `strcpy()` can overflow if source string is longer than `ip_length`
- No null-termination guarantee
- No bounds checking

### AFTER:
```c
cashier_node_ips[i] = calloc(ip_length, sizeof(char));
if (cashier_node_ips[i] == NULL) {
    // Handle allocation failure
    return ERR_MEMORY;
}
const char *ip_str = unstringify(cJSON_Print(cJSON_GetArrayItem(msig_addr_nodes, i)));
if (ip_str != NULL) {
    strncpy(cashier_node_ips[i], ip_str, ip_length - 1);
    cashier_node_ips[i][ip_length - 1] = '\0';  // Ensure null termination
} else {
    free(cashier_node_ips[i]);
    cashier_node_ips[i] = NULL;
    return ERR_INVALID_DATA;
}
```

---

## Issue 2: Memory Leak - Missing Free in Error Paths

### BEFORE (cashier.c:534-549):
```c
hex_data = calloc(tx_data_size * 2, sizeof(char));
retval = chips_extract_data(jstr(argjson, "tx_id"), &hex_data);

if ((retval == OK) && (hex_data)) {
    data = calloc(tx_data_size, sizeof(char));
    hexstr_to_str(hex_data, data);
    player_info = cJSON_CreateObject();
    player_info = cJSON_Parse(data);
    // ... more code ...
}
dlg_info("%s", cJSON_Print(dispute_response));
```
**Problems:**
- `hex_data` allocated but never freed if `retval != OK`
- `data` allocated but never freed
- `player_info` created but overwritten (memory leak)
- No cleanup in error paths

### AFTER:
```c
hex_data = calloc(tx_data_size * 2, sizeof(char));
if (hex_data == NULL) {
    return ERR_MEMORY;
}

retval = chips_extract_data(jstr(argjson, "tx_id"), &hex_data);
char *data = NULL;
cJSON *player_info = NULL;

if (retval == OK && hex_data != NULL) {
    data = calloc(tx_data_size, sizeof(char));
    if (data == NULL) {
        free(hex_data);
        return ERR_MEMORY;
    }
    
    hexstr_to_str(hex_data, data);
    player_info = cJSON_Parse(data);
    if (player_info == NULL) {
        free(hex_data);
        free(data);
        return ERR_INVALID_DATA;
    }
    // ... more code ...
}

// Cleanup
if (hex_data) free(hex_data);
if (data) free(data);
if (player_info) cJSON_Delete(player_info);
dlg_info("%s", cJSON_Print(dispute_response));
```

---

## Issue 3: Unsafe sprintf() - Potential Buffer Overflow

### BEFORE (cashier.c:213-215):
```c
sql_query = calloc(sql_query_size, sizeof(char));
sprintf(sql_query,
    "UPDATE c_tx_addr_mapping set payin_tx_id_status = 0, payout_tx_id = %s where table_id = \"%s\";",
    cJSON_Print(cJSON_GetObjectItem(argjson, "tx_info")), jstr(argjson, "table_id"));
```
**Problems:**
- `sprintf()` can overflow if formatted string exceeds `sql_query_size`
- No bounds checking
- SQL injection risk if input not sanitized

### AFTER:
```c
const char *tx_info_str = cJSON_Print(cJSON_GetObjectItem(argjson, "tx_info"));
const char *table_id_str = jstr(argjson, "table_id");
if (tx_info_str == NULL || table_id_str == NULL) {
    return ERR_INVALID_DATA;
}

// Calculate required size
int required_size = snprintf(NULL, 0,
    "UPDATE c_tx_addr_mapping set payin_tx_id_status = 0, payout_tx_id = %s where table_id = \"%s\";",
    tx_info_str, table_id_str) + 1;

if (required_size > sql_query_size) {
    sql_query = realloc(sql_query, required_size);
    if (sql_query == NULL) {
        return ERR_MEMORY;
    }
}

snprintf(sql_query, required_size,
    "UPDATE c_tx_addr_mapping set payin_tx_id_status = 0, payout_tx_id = %s where table_id = \"%s\";",
    tx_info_str, table_id_str);
```

---

## Issue 4: Double Free Risk - Freeing NULL Pointers

### BEFORE (vdxf.c:794-798):
```c
if (t_player_info == NULL || t_table_info == NULL) {
    free(game_id_str);
    cJSON_Delete(t_player_info);  // t_player_info might be NULL
    cJSON_Delete(t_table_info);   // t_table_info might be NULL
    return false;
}
```
**Problems:**
- `cJSON_Delete(NULL)` is safe, but inconsistent pattern
- Logic error: if one is NULL, we still try to delete both

### AFTER:
```c
if (t_player_info == NULL || t_table_info == NULL) {
    free(game_id_str);
    if (t_player_info != NULL) cJSON_Delete(t_player_info);
    if (t_table_info != NULL) cJSON_Delete(t_table_info);
    return false;
}
```

---

## Issue 5: Inconsistent Error Handling Pattern

### BEFORE (cashier.c:100-102):
```c
legacy_m_of_n_msig_addr = (char *)malloc(strlen(jstr(msig_addr, "address")) + 1);
memset(legacy_m_of_n_msig_addr, 0x00, strlen(jstr(msig_addr, "address")) + 1);
strncpy(legacy_m_of_n_msig_addr, jstr(msig_addr, "address"), strlen(jstr(msig_addr, "address")));
```
**Problems:**
- No check if `malloc()` succeeded
- Calling `strlen()` multiple times (inefficient)
- `memset()` unnecessary if using `strncpy` properly
- No null-termination guarantee

### AFTER:
```c
const char *addr = jstr(msig_addr, "address");
if (addr == NULL) {
    return ERR_INVALID_DATA;
}

size_t addr_len = strlen(addr);
legacy_m_of_n_msig_addr = (char *)malloc(addr_len + 1);
if (legacy_m_of_n_msig_addr == NULL) {
    return ERR_MEMORY;
}

memcpy(legacy_m_of_n_msig_addr, addr, addr_len);
legacy_m_of_n_msig_addr[addr_len] = '\0';
```

---

## Issue 6: Resource Leak - Missing Cleanup in Loop

### BEFORE (cashier.c:253-262):
```c
cashier_node_status = calloc(no_of_cashier_nodes, sizeof(int));
validation_status = calloc(no_of_cashier_nodes, sizeof(int));
cashier_node_ips = calloc(no_of_cashier_nodes, sizeof(char *));

for (int32_t i = 0; i < cJSON_GetArraySize(msig_addr_nodes); i++) {
    cashier_node_status[i] = 0;
    cashier_node_ips[i] = calloc(ip_length, sizeof(char));
    strcpy(cashier_node_ips[i], unstringify(...));
    // If error occurs here, previous allocations not freed
}
```
**Problems:**
- No error handling if `calloc()` fails
- If loop fails mid-way, previous allocations leak
- No cleanup function

### AFTER:
```c
cashier_node_status = calloc(no_of_cashier_nodes, sizeof(int));
validation_status = calloc(no_of_cashier_nodes, sizeof(int));
cashier_node_ips = calloc(no_of_cashier_nodes, sizeof(char *));

if (cashier_node_status == NULL || validation_status == NULL || cashier_node_ips == NULL) {
    // Cleanup on failure
    if (cashier_node_status) free(cashier_node_status);
    if (validation_status) free(validation_status);
    if (cashier_node_ips) free(cashier_node_ips);
    return ERR_MEMORY;
}

for (int32_t i = 0; i < cJSON_GetArraySize(msig_addr_nodes); i++) {
    cashier_node_status[i] = 0;
    cashier_node_ips[i] = calloc(ip_length, sizeof(char));
    if (cashier_node_ips[i] == NULL) {
        // Cleanup all previous allocations
        for (int32_t j = 0; j < i; j++) {
            free(cashier_node_ips[j]);
        }
        free(cashier_node_status);
        free(validation_status);
        free(cashier_node_ips);
        return ERR_MEMORY;
    }
    // ... safe strncpy ...
}
```

---

## Summary of Issues to Fix:

1. **Buffer Safety**: Replace `strcpy()` with `strncpy()` + null termination
2. **Memory Management**: Add proper error handling and cleanup for all allocations
3. **String Formatting**: Replace `sprintf()` with `snprintf()` or calculate required size
4. **Error Handling**: Consistent error handling patterns with proper cleanup
5. **Resource Leaks**: Ensure all error paths free allocated resources
6. **NULL Checks**: Consistent NULL pointer checking before use
7. **Code Duplication**: Extract common patterns into helper functions
8. **Magic Numbers**: Replace hard-coded sizes with named constants

